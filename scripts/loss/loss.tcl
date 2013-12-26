#
# Copyright (c) 1995 The Regents of the University of California.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#	This product includes software developed by the Computer Systems
#	Engineering Group at Lawrence Berkeley Laboratory.
# 4. Neither the name of the University nor of the Laboratory may be used
#    to endorse or promote products derived from this software without
#    specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# @(#) $Header: /cvsroot/nsnam/ns-2/tcl/test/test-suite-tcpVariants.tcl,v 1.32 2007/09/25 04:29:56 sallyfloyd Exp $
#
# To view a list of available tests to run with this script:
# ns test-suite-tcpVariants.tcl
#

source misc_simple.tcl
 
Trace set show_tcphdr_ 1

set wrap 90
set wrap1 [expr $wrap * 512 + 40]

Class Topology

Topology instproc node? num {
    $self instvar node_
    return $node_($num)
}

#
# Links1 uses 8Mb, 5ms feeders, and a 800Kb 10ms bottleneck.
# Queue-limit on bottleneck is 2 packets.
#
Class Topology/net4 -superclass Topology
Topology/net4 instproc init ns {
    $self instvar node_
    set node_(s1) [$ns node]
    set node_(s2) [$ns node]
    set node_(r1) [$ns node]
    set node_(k1) [$ns node]

    $self next
    $ns duplex-link $node_(s1) $node_(r1) 8Mb 0ms DropTail
    $ns duplex-link $node_(s2) $node_(r1) 8Mb 0ms DropTail
    $ns duplex-link $node_(r1) $node_(k1) 800Kb 100ms DropTail
    $ns queue-limit $node_(r1) $node_(k1) 8
    $ns queue-limit $node_(k1) $node_(r1) 8

		$self instvar lossylink_
		set lossylink_ [$ns link $node_(r1) $node_(k1)]
		set em [new ErrorModule Fid]
		set errmodel [new ErrorModel/Periodic]
    $errmodel unit pkt
		$lossylink_ errormodule $em
}


TestSuite instproc finish file {
	exit 0
}

TestSuite instproc emod {} {
        $self instvar topo_
        $topo_ instvar lossylink_
        set errmodule [$lossylink_ errormodule]
        return $errmodule
} 

TestSuite instproc drop_pkts pkts {
    $self instvar ns_
    set emod [$self emod]
    set errmodel1 [new ErrorModel/List]
    $errmodel1 droplist $pkts
    $emod insert $errmodel1
    $emod bind $errmodel1 1
}
 
TestSuite instproc setup {tcptype list} {
	global wrap wrap1
  $self instvar ns_ node_ testName_
	$self setTopo

  ###Agent/TCP set bugFix_ false
	set fid 1
	if {$tcptype == "hybrid"} {
		set wrap $wrap1
			set tcp1 [new Agent/SCTP/Ratehybrid]
			set sink [new Agent/SCTP/RatehybridSink]
			$ns_ attach-agent $node_(s1) $tcp1
			$ns_ attach-agent $node_(k1) $sink
			$tcp1 set fid_ $fid
			$sink set fid_ $fid
			$ns_ connect $tcp1 $sink
			# set up TCP-level connections
			$sink listen ; # will figure out who its peer is
	} elseif {$tcptype == "sctp"} {
		set wrap $wrap1
			set tcp1 [new Agent/SCTP]
			set sink [new Agent/SCTP]
			$ns_ attach-agent $node_(s1) $tcp1
			$ns_ attach-agent $node_(k1) $sink
			$tcp1 set fid_ $fid
			$sink set fid_ $fid
			$ns_ connect $tcp1 $sink
			# set up TCP-level connections
			$sink listen ; # will figure out who its peer is
	} elseif {$tcptype == "tfrc"} {
		set wrap $wrap1
			set tcp1 [new Agent/TFRC]
			set sink [new Agent/TFRCSink]
			$ns_ attach-agent $node_(s1) $tcp1
			$ns_ attach-agent $node_(k1) $sink
			$tcp1 set fid_ $fid
			$sink set fid_ $fid
			$ns_ connect $tcp1 $sink
			# set up TCP-level connections
			$sink listen ; # will figure out who its peer is
	}

	$tcp1 set window_ 28
	set ftp1 [$tcp1 attach-app FTP]
	$ns_ at 1.0 "$ftp1 start"

	$self drop_pkts $list

	#$self traceQueues $node_(r1) [$self openTrace 6.0 $testName_]
	$ns_ at 6.0 "$self cleanupAll $testName_"
	$ns_ run
}

# Definition of test-suite tests

###################################################
## One drop
###################################################
Class Test/hybrid_onedrop -superclass TestSuite
Test/hybrid_onedrop instproc init {} {
	$self instvar net_ test_ guide_
	set net_	net4
	set test_	hybrid_onedrop
	set guide_	"Hybrid, one drop."
	$self next traceFiles
}
Test/hybrid_onedrop instproc run {} {
	$self instvar guide_
	puts "Guide: $guide_"
        $self setup hybrid {16}
}

Class Test/sctp_onedrop -superclass TestSuite
Test/sctp_onedrop instproc init {} {
	$self instvar net_ test_ guide_
	set net_	net4
	set test_	sctp_onedrop
	set guide_	"SCTP, one drop."
	$self next traceFiles
}
Test/sctp_onedrop instproc run {} {
	$self instvar guide_
	puts "Guide: $guide_"
        $self setup sctp {16}
}

Class Test/tfrc_onedrop -superclass TestSuite
Test/tfrc_onedrop instproc init {} {
	$self instvar net_ test_ guide_
	set net_	net4
	set test_	tfrc_onedrop
	set guide_	"TFRC, one drop." 
	$self next traceFiles 
}
Test/tfrc_onedrop instproc run {} {
	$self instvar guide_
	puts "Guide: $guide_"
        $self setup tfrc {16}
}
TestSuite runTest
