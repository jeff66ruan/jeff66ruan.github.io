int msg_recv_whole_msg(struct msg_client *client, struct msg_frag **head, int nonblock,
                          struct timespec *ts)
{
   struct msg_frag *cur_msg;
   struct msg_queue *q;
   struct ext_wait_queue wait;
   struct timeval tv;
   int ret = 0;
   long timeout = 0;
   bool dummy, peek_dummy, sleep_entry;
   bool first_entry = true;
   buffer_bottom_t* bb = NULL;
   *head = NULL;

  if(g_enable_timestamps)
     do_gettimeofday(&tv);

   rcu_read_lock(); // make sure q is not freed (in RCU call back)
   q = client->q;
   if (unlikely(!q)) {
      rcu_read_unlock();
      return -ENOTCONN;
   }
   
   prefetch(q);
   timeout = __prepare_timeout(ts);
   do {
      spin_lock_bh(&q->lock);
      if(likely(first_entry)){
         rcu_read_unlock(); //we can unlock RCU because spin_lock_bh already disabled preempt
         first_entry = false;
      }
      else {
         /* Make sure next message is dummy because other threads may fetch it. */
         if (!__next_msg_is_dummy(q) && ((*head) != NULL)) {
            spin_unlock_bh(&q->lock);
            break;
         }
      }

      if (q->msg_queue_pending_msgs) {
         sleep_entry = false;
         DTRACE(3, "msg_recv_whole_msg(): Msg priority queue not empty.\n");

         cur_msg = __get_first_msg(q, false);
         if (unlikely(cur_msg == NULL)) {
            spin_unlock_bh(&q->lock);
            EPRINTF("Msg Q list empty while counter for msg is %u\n",
               q->msg_queue_pending_msgs);
            return -ERANGE;
         }
         dummy = MSG_IS_DUMMY_MSG(MSGHEADER(cur_msg));
         
         bb = (buffer_bottom_t *)(cur_msg->data);

         q->msg_queue_pending_msgs--;
         
         if (dummy) {
            q->msg_queue_pending_dummy_msgs--;
         }
         
         if (need_input_sync(client, cur_msg)) {
            client->q->msg_queue_pending_wosp_msgs--;
         }

         if (!MSG_IS_SYNC_MSG(bb)){
            q->msg_queue_size -= cur_msg->n_bufs;
            q->msg_queue_msgs--;
            msg_pid_queue_stat_dec(q, bb->process_id, cur_msg->n_bufs);
         }
         
         recv_input_sync_prepare(client, cur_msg);

         // must put behind sync msg send out, for sync msg will use filler2.
         if (!dummy){
            bb->filler2 = (bb->filler2 & 0xffff0000)| get_and_inc_hand_seq(q, bb->process_id);
         }

         /* Take a peek whether the next message is a dummy message. */
         peek_dummy = __next_msg_is_dummy(q);

         spin_unlock_bh(&q->lock);
         recv_input_sync(client, cur_msg);
         ret = 0;
      }
      else {
         cur_msg = NULL;
         dummy = peek_dummy = false;
         sleep_entry = true;
         if (unlikely(nonblock)) {
            DPRINTF(3, "msg_recv_whole_msg(): nonblock, don't sleep\n");
            spin_unlock_bh(&q->lock);
            ret = -EAGAIN;
         }
         else if (unlikely(timeout < 0)) {
            DPRINTF(3, "msg_recv_whole_msg(): timeout < 0: %ld\n", timeout);
            spin_unlock_bh(&q->lock);
            ret = -ETIMEDOUT;
         }
         else {
            wait.task = current;
            wait.state = STATE_NONE;
            DPRINTF(3, "msg_recv_whole_msg(): going to sleep.\n");
            ret = __wq_sleep(&client->q, &timeout, &wait);
            if (ret == 0) {
               cur_msg = wait.head;
               recv_input_sync(client, cur_msg);
               if(unlikely(MSG_IS_DUMMY_MSG(MSGHEADER(cur_msg)))){
                  WPRINTF("Unexpected dummy message received in wait task. From %x.%x.\n",
                     MSGHEADER(cur_msg)->computer,MSGHEADER(cur_msg)->family);
                  dummy = true;
               }
            }
         }
      }
      DTRACE(3, "msg_recv_whole_msg() ret: %d, msg:(%p)\n", ret, cur_msg);

      if (dummy)
         msg_free_msg(&cur_msg);
      else if ((*head) == NULL) //Store the non-dummy message to receiver
         *head = cur_msg;
   } while (((*head == NULL) && !sleep_entry) || (dummy && sleep_entry) || unlikely(peek_dummy));

   /* restore bb->phys_computer because all input-sync related logic is completed here
    restore it so that monitor tools sees compliant data as cIPA */
   if (*head) {
      bb = (buffer_bottom_t *)(*head)->data;
      bb->phys_computer = bb->next_phys_computer;
      if(g_enable_timestamps){
         set_time_rec(*head, TIMEREC_ENTER_IOCTL, &tv);
      }      
   }

   __msg_monitor(*head, MON_POINTS_T_RECEIVE_C);

   return ret;
}

