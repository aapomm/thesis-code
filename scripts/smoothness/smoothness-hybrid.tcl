set ns [new Simulator]

set nf [open smoothness-hybrid.nam w]
$ns namtrace-all $nf

set nd [open smoothness-hybrid.tr w]
$ns trace-all $nd

proc finish {} {
	global ns nf nd
	$ns flush-trace
	close $nf
	close $nd
	exit 0
}

set bottleneck [$ns node]


# create tcp nodes and agents
for {set i 0} {$i < 8} {incr i} {
	# source
	set tcp-src-n($i) [$ns node]	
	set tcp-src($i) [new Agent/TCP/Sack1]
	$ns duplex-link $tcp-src-n($i) $bottleneck 50Mb 2ms DropTail
	$ns attach-agent $tcp-src-n($i) $tcp-src($i)
	# sink
	set tcp-snk-n($i) [$ns node]
	$ns duplex-link $bottleneck $tcp-snk-n($i) 5Mb 20ms DropTail
	$ns queue-limit $bottleneck $tcp-snk-n($i) 300
	set tcp-snk($i) [new Agent/TCPSink]
	$ns attach-agent $tcp-snk-n($i) $tcp-snk($i)

	$ns connect tcp-src($i) tcp-snk($i)
}

# create 8 (hybrid/sctp) nodes
for {set i 0} {$i < 8} {incr i} {
	# source
	set sctp-src-n($i) [$ns node]
	set sctp-src($i) [new Agent/SCTP]
	$ns duplex-link $sctp-src-n($i) $bottleneck 50Mb 2ms DropTail
	$ns attach-agent $sctp-src-n($i) $sctp-src($i)
	# sink
	set sctp-snk-n($i) [$ns node]
	$ns duplex-link $bottleneck $sctp-snk-n($i)  5Mb 20ms DropTail
	$ns queue-limit $bottleneck $sctp-snk-n($i) 300
	set sctp-src($i) [new Agent/SCTP]
	$ns attach-agent $sctp-src-n($i) $sctp-src($i)

	$ns connect sctp-src($i) sctp-snk($i)
}	

# create 2 udp nodes
for {set i 0} {$i < 2} {incr i} {
	# source
	set udp-src-n($i) [$ns node]
	set udp-src($i) [new Agent/UDP]
	$ns duplex-link $udp-src-n($i) $bottleneck 50Mb 2ms DropTail
	$ns attach-agent $udp-src-n($i) $sctp-src($i)
	# sink
	set udp-snk-n($i) [$ns node]
	$ns duplex-link $bottleneck $udp-snk-n($i)  5Mb 20ms DropTail
	$ns queue-limit $bottleneck $udp-snk-n($i) 300
	set udp-src($i) [new Agent/Null]
	$ns attach-agent $sctp-src-n($i) $sctp-src($i)

	$ns connect udp-src($i) udp-snk($i)
}
