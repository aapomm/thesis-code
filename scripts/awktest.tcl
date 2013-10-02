#Create a simulator object

set ns [new Simulator]

 

#Define different colors for data flows (for NAM)

$ns color 1 Blue

$ns color 2 Red

 

#Open the NAM trace file

set nf [open out.nam w]

$ns namtrace-all $nf

 

#Open the traffic trace file to record all events

set nd [open out.tr w]

$ns trace-all $nd

 

#Define a 'finish' procedure

proc finish {} {

        global ns nf nd

        $ns flush-trace

        #Close the NAM trace file

        close $nf

        close $nd

        #Execute NAM on the trace file

        exec nam out.nam &

        exit 0

}

 

#Create four nodes


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

 

#Setup a UDP connection

set udp [new Agent/SCTP]

$ns attach-agent $n1 $udp

set null [new Agent/SCTP]

$ns attach-agent $n3 $null

$ns connect $udp $null

$udp set fid_ 2

 

#Setup a CBR over UDP connection

set cbr [new Application/Traffic/CBR]

$cbr attach-agent $udp

$cbr set type_ CBR

$cbr set packet_size_ 1000

$cbr set rate_ 0.5mb

$cbr set random_ false

 

#Schedule events for the CBR and FTP agents

$ns at 0.1 "$cbr start"

$ns at 4.5 "$cbr stop"

 

#Detach tcp and sink agents (not really necessary)

 

#Call the finish procedure after 5 seconds of simulation time

$ns at 5.0 "finish"

 

#Print CBR packet size and interval

puts "CBR packet size = [$cbr set packet_size_]"

puts "CBR interval = [$cbr set interval_]"

 

#Run the simulation

$ns run
