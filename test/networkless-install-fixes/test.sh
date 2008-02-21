#!/bin/sh

OPTS="-o Dir::Etc::sourcelist=./sources.test.list -o Acquire::http::timeout=20"

# setup
unset http_proxy
iptables --flush

echo "No network at all"
ifdown eth0 
time apt-get update $OPTS 2>&1 |grep system
ifup eth0
echo ""

echo "no working DNS (port 53 DROP)"
iptables -A OUTPUT -p udp --dport 53 -j DROP
time apt-get update $OPTS 2>&1 |grep system
iptables --flush
echo ""

echo "DNS but no access to archive.ubuntu.com (port 80 DROP)"
iptables -A OUTPUT -p tcp --dport 80 -j DROP
time apt-get update $OPTS 2>&1 |grep system
iptables --flush
echo ""
