BEGIN {
  bytes_recvd[16];
  total_size[16];
	number_of_nodes = 8;
  tcp_bytes = 0;
  hybrid_bytes = 0;
  interval = 1;
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
      if (action == "r" && to != 0 && to != 1)
      {
        total_size[to] += pkt_size;
        bytes_recvd[to] += pkt_size;
			}
    }
  else {
    for (i = 2; i < (number_of_nodes * 2) + 2; i++) {
			if(i % 2 == 1)
			{
					printf "%lf %lf\n", current_time_instance, bytes_recvd[i] >> (i + ".out")
					bytes_recvd[i] = 0;
			}
    }
		for(i = (number_of_nodes * 2) + 2; i < (number_of_nodes * 4) + 2; i++) {
			if(i % 2 == 1)
			{
					printf "%lf %lf\n", current_time_instance, bytes_recvd[i] >> (i + ".out")
					bytes_recvd[i] = 0;
			}
		}
    current_time_instance = nxt_time_instance;
    nxt_time_instance += interval;
  }
}
END {
    for (i = 2; i < (number_of_nodes * 2) + 2; i++) {
			if (i % 2 == 1) tcp_bytes += total_size[i];
    }
		for(i = (number_of_nodes * 2) + 2; i < (number_of_nodes * 4) + 2; i++) {
      if (i % 2 == 1)  hybrid_bytes += total_size[i];
		}
  printf("Fairness: %lf\n", hybrid_bytes / (tcp_bytes + hybrid_bytes));
}
