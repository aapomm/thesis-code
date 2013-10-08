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
        exec nam out.nam &
        exit 0
}

############Create nodes#############################
set sctp0_n [$ns node]
$sctp0_n label "SCTP"
$sctp0_n color "red"

set sctp1_n [$ns node]
$sctp1_n label "SCTP"
$sctp1_n color "red"

set tcp0_n [$ns node]
$tcp0_n label "TCP"
$tcp0_n color "blue"

set tcp1_n [$ns node]
$tcp0_n label "TCP"
$tcp0_n color "blue"

set int0_n [$ns node]
set int1_n [$ns node]

set recv_n [$ns node]

for {set i 0} {$i < 20} {incr i} {
	set n($i) [$ns node]
}
#####################################################


########Create links between the nodes###############
for {set i 0} {$i < 20} {incr i} {
	$ns duplex-link $n($i) $int0_n 10Mb 10ms DropTail
}
$ns duplex-link $int0_n $int1_n 10Mb 10ms DropTail
$ns duplex-link $int1_n $recv_n 10Mb 10ms DropTail

$ns duplex-link $sctp0_n $int0_n 10Mb 10ms DropTail
$ns duplex-link $sctp1_n $int1_n 10Mb 10ms DropTail

$ns duplex-link $tcp0_n $int0_n 10Mb 10ms DropTail
$ns duplex-link $tcp1_n $int1_n 10Mb 10ms DropTail
#######################################################

#Set Queue Size of link (n2-n3) to 10
# $ns queue-limit $int0_n $int1_n 10

# #Monitor the queue for link (n2-n3). (for NAM)
$ns duplex-link-op $int0_n $int1_n queuePos 0.5


$ns duplex-link-op $sctp0_n $int0_n orient left
$ns duplex-link-op $sctp1_n $int1_n orient left
$ns duplex-link-op $tcp0_n $int0_n orient right
$ns duplex-link-op $tcp1_n $int1_n orient right
$ns duplex-link-op $int0_n $int1_n orient up
$ns duplex-link-op $int0_n $int1_n orient up
$ns duplex-link-op $int1_n $recv_n orient up

####################Setup agents#####################
for {set i 0} {$i < 20} {incr i} {
	set udp($i) [new Agent/UDP]
	$ns attach-agent $n($i) $udp($i)
}

set sctp0 [new Agent/SCTP]
$ns attach-agent $sctp0_n $sctp0

set sctp1 [new Agent/SCTP]
$sctp1 set fid_ 1
$ns attach-agent $sctp1_n $sctp1

set tcp0 [new Agent/TCP]
$ns attach-agent $tcp0_n $tcp0

set tcp1 [new Agent/TCPSink]
$tcp1 set fid_ 2
$ns attach-agent $tcp1_n $tcp1

set udp_recv [new Agent/UDP]
$ns attach-agent $recv_n $udp_recv
#####################################################


##################Setup traffic#####################
# PARETO TRAFFIC
for {set i 0} {$i < 20} {incr i} {
	set traf($i) [new Application/Traffic/Pareto]
	$traf($i) set shape_ 1.0
	$traf($i) attach-agent $udp($i)
	$traf($i) set rate_ 0.4mb
}

set sctp_traf [new Application/Traffic/Pareto]
$sctp_traf set shape_ 1.0
$sctp_traf set rate_ 0.4mb
$sctp_traf attach-agent $sctp0

set tcp_traf [new Application/Traffic/Pareto]
$tcp_traf set shape_ 1.0
$tcp_traf set rate_ 0.4mb
$tcp_traf attach-agent $tcp0

###################################################

##################Connect agents###################
for {set i 0} {$i < 20} {incr i} {
	$ns connect $udp($i) $udp_recv
}
$ns connect $sctp0 $sctp1
$ns connect $tcp0 $tcp1
###################################################

################Run traffic########################
for {set i 0} {$i < 20} {incr i} {
	$ns at 0.0 "$traf($i) start"
	$ns at 10.0 "$traf($i) stop"
}
$ns at 0.0 "$sctp_traf start"
$ns at 0.0 "$tcp_traf start"
$ns at 10.0 "$sctp_traf stop"
$ns at 10.0 "$tcp_traf stop"

###################################################

$ns at 15.0 "finish"

#Run the simulation
$ns run
