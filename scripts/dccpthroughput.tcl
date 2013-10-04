set ns [new Simulator]
#set nf [open out.nam w]

set f0 [open out0.tr w]
set f1 [open out1.tr w]
set f2 [open out2.tr w]

#$ns namtrace-all $nf

proc finish {} {
        global f0 f1 f2
        #Close the output files
        close $f0
        close $f1
        close $f2
        #Call xgraph to display the results
        #exec xgraph out0.tr -geometry 800x400 &
        exec nam out.nam &
        exit 0
}

proc attach-expoo-traffic { node sink size interval } {
	#Get an instance of the simulator
	set ns [Simulator instance]

	#Create a UDP agent and attach it to the node
	set source [new Agent/DCCP/TCPlike]
	$ns attach-agent $node $source

	#Create an Expoo traffic agent and set its configuration parameters
	set traffic [new Application/Traffic/CBR]
	$traffic set packetSize_ $size
	$traffic set interval_ $interval
        
        # Attach traffic source to the traffic generator
        $traffic attach-agent $source
	#Connect the source and the sink
	$ns connect $source $sink
	return $traffic
}

proc record {} {
        global sink0 sink1 sink2 f0 f1 f2
        #Get an instance of the simulator
        set ns [Simulator instance]
        #Set the time after which the procedure should be called again
        set time 0.5
        #How many bytes have been received by the traffic sinks?
        set bw0 [$sink0 set bytes_]
        set bw1 [$sink1 set bytes_]
        set bw2 [$sink2 set bytes_]
        puts "bw0: $bw0"
        puts "bw1: $bw1"
        puts "bw2: $bw2"
        #Get the current time
        set now [$ns now]
        #Calculate the bandwidth (in MBit/s) and write it to the files
        puts $f0 "$now [expr $bw0/$time*8/1000000]"
        puts $f1 "$now [expr $bw1/$time*8/1000000]"
        puts $f2 "$now [expr $bw2/$time*8/1000000]"
        #Reset the bytes_ values on the traffic sinks
        $sink0 set bytes_ 0
        $sink1 set bytes_ 0
        $sink2 set bytes_ 0
        #Re-schedule the procedure
        $ns at [expr $now+$time] "record"
}

set n0 [$ns node]
set n1 [$ns node]
set n2 [$ns node]
set n3 [$ns node]
set n4 [$ns node]
set sink0 [new Agent/DCCP/TCPlike]
set sink1 [new Agent/DCCP/TCPlike]
set sink2 [new Agent/DCCP/TCPlike]

$ns attach-agent $n4 $sink0
$ns attach-agent $n4 $sink1
$ns attach-agent $n4 $sink2

$ns duplex-link $n0 $n3 1Mb 100ms DropTail
$ns duplex-link $n1 $n3 1Mb 100ms DropTail
$ns duplex-link $n2 $n3 1Mb 100ms DropTail
$ns duplex-link $n3 $n4 1Mb 100ms DropTail

set source0 [attach-expoo-traffic $n0 $sink0 200 0.05]
set source1 [attach-expoo-traffic $n1 $sink1 200 0.05]
set source2 [attach-expoo-traffic $n2 $sink2 200 0.05]

$ns at 0.0 "record"
$ns at 10.0 "$source0 start"
$ns at 10.0 "$source1 start"
$ns at 10.0 "$source2 start"
$ns at 10.0 "$sink0 listen"
$ns at 10.0 "$sink1 listen"
$ns at 10.0 "$sink2 listen"
$ns at 50.0 "$source0 stop"
$ns at 50.0 "$source1 stop"
$ns at 50.0 "$source2 stop"
$ns at 60.0 "finish"

$ns run
