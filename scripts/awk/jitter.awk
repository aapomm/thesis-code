BEGIN {
     highest_packet_id = 0;
     prev_highest = 0;
}

{
   action = $1;
   time = $2;
   from = $3;
   to = $4;
   type = $5;
   pktsize = $6;
   flow_id = $8;
   src = $9;
   dst = $10;
   seq_no = $11;
   packet_id = $12;

   if ( packet_id > highest_packet_id )
          prev_highest = highest_packet_id;
         highest_packet_id = packet_id;

   if ( start_time[packet_id] == 0){
        start_time[packet_id] = time;
  }

  else if ( flow_id == 1 && action == "r" && to == 4) {
         end_time[packet_id] = time;

   } else {
      end_time[packet_id] = -1;
   }

}                                                       

END {
    prev_pkt = 0;
    count = 1;
    for ( packet_id = 1; packet_id <= highest_packet_id; packet_id++ ) {
      if(end_time[packet_id] != -1){
       end = end_time[packet_id];
       if(prev_pkt == 0) prev_pkt = end;
       delay_interval = end - prev_pkt;
       printf("%d %lf\n", packet_id, delay_interval);

       prev_pkt = end;
       count++;
      }
   }
}
