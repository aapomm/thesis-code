set ns [new Simulator]

$ns color 1 Blue
$ns color 2 Red

set nf [open out.nam w]
$ns namtrace-all $nf

set nd [open SWAGGAZ.tr w]
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

set sctp1_n [$ns node]
$sctp1_n label "SCTP1"

set int0_n [$ns node]
set int1_n [$ns node]

set sctp0_n [$ns node]
$sctp0_n label "SCTP0"

$ns duplex-link $sctp0_n $int0_n 100Mb 25ms DropTail
$ns duplex-link $sctp1_n $int1_n 100Mb 25ms DropTail

$ns duplex-link $int0_n $int1_n 5Mb 25ms DropTail
# $ns duplex-link-op $int0_n $int1_n queuePos 0.5

set sctp0 [new Agent/SCTP/Ratehybrid]
$sctp0 set fid_ 1
$ns attach-agent $sctp0_n $sctp0
#$ns multihome-attach-agent $sctp0_n $sctp0

set sctp1 [new Agent/SCTP/RatehybridSink]
$ns attach-agent $sctp1_n $sctp1

set sctp_traf [new Application/Traffic/CBR]
$sctp_traf set packetSize_ 1000
$sctp_traf set rate_ 5mb
$sctp_traf attach-agent $sctp0

$ns connect $sctp0 $sctp1

$ns at 0.5 "$sctp_traf start"
$ns at 7.5 "$sctp_traf stop"

$ns at 20.5 "finish"

puts "cbr rate: [$sctp_traf set interval_]"

$ns run
