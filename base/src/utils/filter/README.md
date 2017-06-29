# ipxfilter documentation

## Authors
Imrich Štoffa, Tomáš Podermansky, Lukás Huták

## Purpose

## Syntax
Superseds nfdump filter syntax, in addition, any ipfix field name can be used as field name

Use exist keyword to check for existence of field in record eg.: "exist mac",
true expression: "any"...

Before each field name one can use direction qualifiers 
src, prev, in, ingress, dst, next, out, egress. Note that theese are concatenated as prefixes to names and sent like that to lookup func

Another "src and dst", "src or dst" and vice versa are transformed to flag whether multinode (name that identifies multiple data fields) is OR or AND tree. Use and if all fields in mulinode must match, or is default. OR is also used for lookup function configured multinodes

## Valid field names 
* inet 
* ip, net, host 
* mask
* if
* port
* icmp-type
* icmp-code
* engine-type
* engine-code
* as
* vlan
* flags
* next ip
* bgpnext ip
* router ip
* mac
* mpls {label} 1-10 -untested
* mpls eos -untested
* mpls exp 1-10 -untested
* packets
* bytes
* flows
* tos
* duration
* bps
* pps
* bpp
* xevent -should work
* nat event -sw
* nip -sw
* nport -sw
* vrfid -sw

## Field values
So far special signs in values is not supported. t'is Subject to change.

# Design
...

# Extensibility
...
