#Create a simulator object
set ns [new Simulator]
 
#Define different colors for data flows (for NAM)
$ns color 1 Blue
$ns color 2 Red

#Open the NAM trace file
set nf [open out.nam w]
$ns namtrace-all $nf

#Open the traffic trace file to record all events
set nd [open SCTP.tr w]
$ns trace-all $nd

#Define a 'finish' procedure
proc finish {} {
        global ns nf nd
        $ns flush-trace
        #Close the NAM trace file
        close $nf
        close $nd
        #Execute NAM on the trace file
        #exec nam out.nam &
        exit 0
}

#Create nodes
set n1 [$ns node]
set n2 [$ns node]
set n3 [$ns node]

#Create links between the nodes
$ns duplex-link $n1 $n2 2Mb 10ms DropTail
$ns duplex-link $n2 $n3 1.7Mb 20ms DropTail

#Set Queue Size of link (n2-n3) to 10
$ns queue-limit $n2 $n3 10

#Give node position (for NAM)
$ns duplex-link-op $n1 $n2 orient right-up
$ns duplex-link-op $n2 $n3 orient right

#Monitor the queue for link (n2-n3). (for NAM)
$ns duplex-link-op $n2 $n3 queuePos 0.5

#Setup a agents
set udp [new Agent/SCTP]
#$udp set numUnrelStreams_ 1
$ns attach-agent $n1 $udp

set null [new Agent/SCTP]
#$null set numUnrelStreams_ 1
$ns attach-agent $n3 $null
$ns connect $udp $null
$udp set fid_ 2
$null set fid_ 1

#Setup a CBR
set cbr [new Application/Traffic/CBR]
$cbr attach-agent $udp
$cbr set type_ packet
$cbr set packet_size_ 1000
$cbr set rate_ 0.5mb
#$cbr set interval_ 0.088

# set em [new ErrorModel]
# $em set rate_ 0.01
# $em unit pkt
# $em ranvar [new RandomVariable/Uniform]
# $em drop-target [new Agent/Null]
# $ns lossmodel $em $n2 $n3

#Schedule events
$ns at 1.0 "$cbr start"
#$ns at 1.0 "$null listen"
$ns at 12.0 "$cbr stop"

#Call the finish procedure after 5 seconds of simulation time
$ns at 15.0 "finish"

#Print CBR packet size and interval
puts "CBR packet size = [$cbr set packet_size_]"
puts "CBR interval = [$cbr set interval_]"
puts "CBR rate = [$cbr set rate_]" 

#Run the simulation
$ns run
