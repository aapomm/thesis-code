BEGIN {
  bytes_recvd[16];
  total_size[16];
  tcp_bytes = 0;
  hybrid_bytes = 0;
  interval = 0.25;
  current_time_instance = 0;
  nxt_time_instance = current_time_instance + interval;
  for (i = 0; i < 16; i++) {
    bytes_recvd[i] = 0;
    total_size[i] = 0;
  }
}
{
  action = $1;
  time = $2;
  from = $3; 
  to = $4;
  type = $5; 
  pkt_size = $6;
  flow_id = $8;
  src = $9;
  dst = $10;
  sequence_n0 = $11;
  pkt_id = $12;

  if (time < nxt_time_instance)
    { 
      if (action == "r")
      {
        total_size[to] += pkt_size;
        bytes_recvd[to] += 1;
			}
    }
  else {
    current_time_instance = nxt_time_instance;
    nxt_time_instance += interval;
    for (i = 0; i < 16; i++) {
      printf "%lf %lf\n", current_time_instance, bytes_recvd[i] >> (i + ".out")
      bytes_recvd[i] = 0;
    }
  }
}
END {
  for (i = 0; i < 16; i++) {
    if (i < 8) tcp_bytes += total_size[i];
    else hybrid_bytes += total_size[i];
  }
  printf("Fairness: %lf\n", hybrid_bytes / (tcp_bytes + hybrid_bytes));
}
