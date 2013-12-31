BEGIN {
  bytes_recvd1 = 0;
  total_size1 = 0;
  throughput = 0;
  interval = 0.25;
  current_time_instance = 0;
  nxt_time_instance = current_time_instance + interval;
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
        total_size1 += pkt_size;
        bytes_recvd1 += 1;
      }
    }
  else {
    current_time_instance = nxt_time_instance;
    nxt_time_instance += interval;
    printf("%lf %lf\n", current_time_instance, (total_size1*8)/(interval*1000000));
    #printf("%lf %lf\n", current_time_instance, (total_size1*8)/(interval*1000000));
#printf("%d %f\n", bytes_recvd1, (total_size1*8)/(interval*1000000));
    total_size1 = 0;
  }
}
END {
  #printf("total sctp: %d\n", bytes_recvd1);
  #printf("total udp: %d\n", bytes_recvd2);
  #printf("total tcp: %d\n", bytes_recvd3);

  #printf("total sctp tput: %lf\n", (total_size1*8/(nxt_time_instance*1000000)));
  #printf("total udp tput: %lf\n", (total_size2*8/(nxt_time_instance*1000000)));
  #printf("total tcp tput: %lf\n", (total_size3*8/(nxt_time_instance*1000000)));
}
