set ns [new Simulator]

$ns color 1 Blue
$ns color 2 Red

set nf [open baseline-tfrc.nam w]
$ns namtrace-all $nf

set nd [open baseline-tfrc.tr w]
$ns trace-all $nd

proc finish {} {
        global ns nf nd
        $ns flush-trace
        close $nf
        close $nd
        # exec nam out.nam &
        exit 0
}
$ns node-config -macType Mac/802_3

set tcp0_n [$ns node]
set tcp1_n [$ns node]
set udp0_n [$ns node]
set udp1_n [$ns node]
# set sctp0_n [$ns node]
set sctp1_n [$ns node]
$sctp1_n label "SCTP1"

set int0_n [$ns node]
set int1_n [$ns node]

#### FOR MULTIHOMING ##################################
set sctp0_n [$ns node]
$sctp0_n label "SCTP0"
# set host0_if0 [$ns node]
# set host0_if1 [$ns node]

# $ns multihome-add-interface $sctp0_n $host0_if0
# $ns multihome-add-interface $sctp0_n $host0_if1

# $ns duplex-link $sctp0_n $host0_if0 100Mb 25ms DropTail
# $ns duplex-link $sctp0_n $host0_if1 100Mb 25ms DropTail
# $ns duplex-link $host0_if0 $int0_n 100Mb 25ms DropTail
# $ns duplex-link $host0_if1 $int0_n 100Mb 25ms DropTail
###########################################################

$ns duplex-link $sctp0_n $int0_n 100Mb 25ms DropTail
$ns duplex-link $udp0_n $int0_n 100Mb 25ms DropTail
$ns duplex-link $tcp0_n $int0_n 100Mb 25ms DropTail

$ns duplex-link $sctp1_n $int1_n 100Mb 25ms DropTail
$ns duplex-link $udp1_n $int1_n 100Mb 25ms DropTail
$ns duplex-link $tcp1_n $int1_n 100Mb 25ms DropTail

$ns duplex-link $int0_n $int1_n 100Mb 25ms DropTail
$ns queue-limit $int0_n $int1_n 4
# $ns duplex-link-op $int0_n $int1_n queuePos 0.5

set sctp0 [new Agent/TFRC]
$sctp0 set fid_ 1
$ns attach-agent $sctp0_n $sctp0
#$ns multihome-attach-agent $sctp0_n $sctp0

set sctp1 [new Agent/TFRCSink]
$ns attach-agent $sctp1_n $sctp1

set udp0 [new Agent/UDP]
$udp0 set fid_ 2
$ns attach-agent $udp0_n $udp0

set udp1 [new Agent/Null]
$ns attach-agent $udp1_n $udp1

set tcp0 [new Agent/TCP]
$tcp0 set fid_ 3
$ns attach-agent $tcp0_n $tcp0

set tcp1 [new Agent/TCPSink/DelAck]
$ns attach-agent $tcp1_n $tcp1

set tcp_traf [new Application/FTP]
# $tcp_traf set packetSize_ 1000
# $tcp_traf set rate_ 5mb
$tcp_traf attach-agent $tcp0

set udp_traf [new Application/Traffic/CBR]
# $udp_traf set packetSize_ 1000
$udp_traf set rate_ 0.2mb
$udp_traf attach-agent $udp0

set sctp_traf [new Application/Traffic/CBR]
$sctp_traf set packetSize_ 1000
$sctp_traf set rate_ 5mb
$sctp_traf attach-agent $sctp0

$ns connect $sctp0 $sctp1
$ns connect $tcp0 $tcp1
$ns connect $udp0 $udp1

$ns at 0.5 "$tcp_traf start"
$ns at 0.5 "$udp_traf start"
$ns at 0.5 "$sctp_traf start"
$ns at 7.5 "$tcp_traf stop"
$ns at 7.5 "$udp_traf stop"
$ns at 7.5 "$sctp_traf stop"

$ns at 7.5 "finish"

puts "cbr rate: [$sctp_traf set interval_]"

$ns run
