/**
 * Aquire the q's lock need RCU read lock proctect because the q can be release
 *   in the mid without that proctect. After the q's lock is got, rcu read lock
 *   is released because q cannot be released anymore
 *
 * return NULL: failure
 * Otherwise: pointer to q is returned and q is locked
 */
STATIC INLINE struct msg_queue *client_q_lock_protect(struct msg_client *client) {
  struct msg_queue *q = NULL;

  rcu_read_lock();
  q = client->q;
  if (unlikely(!q)) {
    rcu_read_unlock();
    return NULL;
  }                   
  spin_lock_bh(&q->lock);
  rcu_read_unlock();     
  return q;
}

#define client_q_unlock(q) spin_unlock_bh(&q->lock)


/**
 * Get message from pending queue.
 *
 * Caller should hold the q's lock and the q shouldn't be empty
 * 
 * The q's lock will be freed in this function when it returns
 * 
 * Return: 0 is successful otherwise failure
 * Output: In case of success, *head helds the pointer of the received msg
 */
STATIC INLINE int __get_a_msg_from_q(struct msg_client *client, struct msg_frag **head)
{
  bool dummy;
  buffer_bottom_t     * bb = NULL;
  struct msg_queue * q = client->q;
  struct msg_frag *cur_msg = NULL;

  cur_msg = __get_first_msg(q, false);

  if (unlikely(!cur_msg)) {
    EPRINTF("Msg Q list empty while counter for msg is %u\n", q->msg_queue_pending_msgs);
    client_q_unlock(q);
    return -ERANGE;
  }

  q->msg_queue_pending_msgs--;
  dummy = MSG_IS_DUMMY_MSG(MSGHEADER(cur_msg));
  if (dummy) {
    q->msg_queue_pending_dummy_msgs--;
  }

  if (need_input_sync(client, cur_msg)) {
    client->q->msg_queue_pending_wosp_msgs--;
  }

  bb = (buffer_bottom_t *)(cur_msg)->data;
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

  client_q_unlock(q);
  *head = cur_msg;
  return 0;
}


/**
 * Waiting for new message, it will sleep while no message is arrival.
 * Caller should hold the q's lock
 * The __wq_sleep will free the q's lock when it returns from.
 */
STATIC INLINE int __waiting_for_new_msg(struct msg_queue *q, int nonblock, 
			    struct timespec *ts, struct msg_frag **head)
{
  int    ret = 0;
  long   timeout = 0;
  struct ext_wait_queue wait;

  *head = NULL;
  timeout = __prepare_timeout(ts);
  if (unlikely(nonblock)) {
    DPRINTF(3, "%s(): nonblock, don't sleep\n", __func__);
    spin_unlock_bh(&q->lock);
    ret = -EAGAIN;
  }
  else if (unlikely(timeout < 0)) {
    DPRINTF(3, "%s(): timeout < 0: %ld\n", __func__, timeout);
    spin_unlock_bh(&q->lock);
    ret = -ETIMEDOUT;
  }
  else {
    wait.task = current;
    wait.state = STATE_NONE;
    DPRINTF(3, "%s(): going to sleep.\n", __func__);
    ret = __wq_sleep(&q, &timeout, &wait);
    if (ret == 0) {
      *head = wait.head;
    }
  }

  return ret;
}


/**
 * return 0 until it received a non dummy message, the recevied msg is saved
 *   in the parameter cur_msg. If it received a dummy message, it handles 
 *   this dummy message and back to receive next message again 
 * 
 * return non-zero if there are errors
 */
STATIC INLINE int msg_recv_a_nondummy_msg(struct msg_client *client, 
			       struct msg_frag * cur_msg, int nonblock,
			       struct timespec *ts)
{
  bool dummy = false; // the received msg is a dummy message
  int  ret = 0;
  struct msg_queue * q;

  cur_msg = NULL;
  do {  /* Get first un-dummy messages. */
    q = client_q_lock_protect(client);
    if (unlikely(!q)){
      return -ENOTCONN;
    }

    if (q->msg_queue_pending_msgs) {
      DTRACE(3, "%s(): Msg priority queue NOT empty.\n", __func__);
      ret = __get_a_msg_from_q(client, &cur_msg);
    }
    else {
      DTRACE(3, "%s(): Msg priority queue empty.\n", __func__);
      ret = __waiting_for_new_msg(q, nonblock, ts, &cur_msg);

    }

    if (unlikely(ret != 0)) {
      return ret;
    }
     
    recv_input_sync(client, cur_msg);

    dummy = MSG_IS_DUMMY_MSG(MSGHEADER(cur_msg));
    if (dummy) {
      msg_free_msg(&cur_msg);
    }
  } while (dummy);

  // a non dummy message is received, return OK
  return 0;
}


/**
 * exhaust all dummy messages in the queue until the queue is empty or the head
 * message isn't a dummy message.
 *
 * Up to now, no need to know whether this function succeeds or not. Therefore,
 * no error code is returned even if there are errors
 */
STATIC INLINE void exhaust_dummy_msgs_from_queue(struct msg_client *client)
{
  struct msg_queue * q;
  struct msg_frag * cur_msg = 0;

  for(;;) {
    q = client_q_lock_protect(client);
    if (unlikely(!q)) {
      break;
    }

    if (!__next_msg_is_dummy(q)) {
      client_q_unlock(q);
      break;
    }

    if (unlikely( __get_a_msg_from_q(client, &cur_msg))) {
      break;
    }

    msg_free_msg(&cur_msg);
  }
}

/*
 *
 */
int msg_recv_whole_msg(struct msg_client *client, struct msg_frag **head,
			  int nonblock, struct timespec *ts)
{
  int  ret = 0;
  struct timeval tv;
  buffer_bottom_t * bb = NULL;
  struct msg_frag * cur_msg = 0;

  *head = NULL;

  if(g_enable_timestamps)
    do_gettimeofday(&tv);

  ret = msg_recv_a_nondummy_msg(client, cur_msg, nonblock, ts);
  if (unlikely(!ret)){
    return ret;
  }

  /* restore bb->phys_computer because all input-sync related logic is completed here
   * restore it so that monitor tools sees compliant data as cIPA */
  *head = cur_msg;
  cur_msg = NULL;
  bb = (buffer_bottom_t *)(*head)->data;
  bb->phys_computer = bb->next_phys_computer;

  if(g_enable_timestamps){
    set_time_rec(*head, TIMEREC_ENTER_IOCTL, &tv);
  }      

  /* exhaust dummy msgs before give the received msg to user 
   * for better latency of relative messages
   */
  exhaust_dummy_msgs_from_queue(client);
  
  return ret;
}

