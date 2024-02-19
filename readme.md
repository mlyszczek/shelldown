shelldown
=========

Program translates new rpc/json API back to good ol' mqtt topics. Tired
of parsing jsons where you could simply use mqtt payload with value, just
like with old shellies? Tired of being forced to use hardcoded mqtt topics?
That program is a solution to that.

How it works, what it does
==========================

New shellies will be sending messages like these:

```
shellyplus1pm-4417939a5610/events/rpc {"src":"shellyplus1pm-4417939a5610","dst":"shellyplus1pm-4417939a5610/events","method":"NotifyStatus","params":{"ts":1708326851.41,"switch:0":{"id":0,"output":true,"source":"MQTT","voltage":224.18}}}
shellyplus1pm-4417939a5610/events/rpc {"src":"shellyplus1pm-4417939a5610","dst":"shellyplus1pm-4417939a5610/events","method":"NotifyStatus","params":{"ts":1708326852.50,"switch:0":{"id":0,"apower":918.63}}}
shellyplus1pm-4417939a5610/events/rpc {"src":"shellyplus1pm-4417939a5610","dst":"shellyplus1pm-4417939a5610/events","method":"NotifyStatus","params":{"ts":1708326854.50,"switch:0":{"id":0,"apower":1800.50}}}
shellyplus1pm-4417939a5610/events/rpc {"src":"shellyplus1pm-4417939a5610","dst":"shellyplus1pm-4417939a5610/events","method":"NotifyStatus","params":{"ts":1708326875.04,"switch:0":{"id":0,"apower":0,"output":false,"source":"MQTT","voltage":0}}}
```

And **shelldown** will simply translate it back to

```
/iot/saloon/heat/relay/0 on
/iot/saloon/heat/relay/0/voltage 224.18
/iot/saloon/heat/relay/0/power 918.63
/iot/saloon/heat/relay/0/power 1800.50
/iot/saloon/heat/relay/0/power 0.00
/iot/saloon/heat/relay/0 off
/iot/saloon/heat/relay/0/voltage 0.00
```

It's not 1:1, if shelly sends multiple readings in a single rpc, **shelldown**
will emit multiple mqtt messages - one for each reading found in rpc message.

You can also control relays with simple mqtt commands

```
mosquitto_pub -t /iot/saloon/heat/relay/0/command -m on
mosquitto_pub -t /iot/saloon/heat/relay/0/command -m off
```

It can also republish messages from v1 shellies on different topics

```
shellies/shellyswitch25-485519032A7E/temperature 54.69
shellies/shellyswitch25-485519032A7E/temperature_f 130.44
shellies/shellyswitch25-485519032A7E/roller/0/pos 91
shellies/shellyswitch25-485519032A7E/roller/0/power 0.00

/iot/office/blinds/0/temperature 54.69
/iot/office/blinds/0/temperature_f 130.44
/iot/office/blinds/0/roller/0/pos 91
/iot/office/blinds/0/roller/0/power 0.00
```

This can usually be done by chaning topic prefix in shelly options, but it's
in just for convenience.

Configuration
=============

All you gotta do, is list shellies and expected topic to **/etc/shelldown-id-map**

```
# comments are allowed, when # is first character
shellyplus1pm-7c87ce65bd9c      office/heat
shellyplus1pm-441793941eac      outdoor/ac-office
shellyplus1pm-441793d47f34      outdoor/ac-second
shellyplug-s-c8c9a3b8fde8       bathroom1/boiler
shellyplug-s-6E2303             office/rack/power
shellyem3-C45BBE7890ED          garage/power-monitor
shellyem-C45BBE7781F3           outdoor/ac-main
shellyplusi4-083af200b274       saloon/buttons/0
```

First column is your shelly id, which can be found at the bottom of the
shelly http website. You also need to prepend this with shelly device.
First column is basically first part of topic on which shelly publishes.
You can grab full topic with (it may take some time for shelly to send
any messages).

```
$ mosquitto_sub -v -t +/events/rpc | grep 4417939a5610 --line-buffered  | cut -f1 -d/
shellyplus1pm-4417939a5610
```

And the second column is on which topic you want message to be republished.
All of them can later be prepended with own prefix by passing **-t<prefix>**
to shelldown program.

When you start without it, default prefix of 'shellies/' will be used.

```
$ shelldown
(no -t passed, 'shellies/' will be used, so you will see messages like)
shellies/office/heat/relay/0/power 10

$ shelldown -t own/prefix
own/prefix/office/heat/relay/0/power 10
```

Implemented APIs
================

**$id** is what is in second column of shelldown-id-map configuration file.

shelly 1pm
-----------

### implemented
| api                              | description |
| -------------------------------- | ----------- |
| shellies/$id/relay/0             | to report status: on, off or overpower (the latter only for Shelly1PM)
| shellies/$id/relay/0/command     | accepts on, off or toggle and applies accordingly
| shellies/$id/relay/0/power       | reports instantaneous power in Watts
| shellies/$id/temperature         | reports internal device temperature in °C
| shellies/$id/temperature_f       | reports internal device temperature in °F
| shellies/$id/temperature_status  | reports Normal, High, Very High

### implemented (but not existing in gen1)
| api                            | description |
| ------------------------------ | ----------- |
| shellies/$id/relay/0/voltage  | last measured voltage in Volts

### not implemented
| api                                   | description |
| ------------------------------------- | ----------- |
| shellies/$id/input/0                  | reports the state of the SW terminal
| shellies/$id/input_event/0            | reports input event and event counter, e.g.: {"event":"S","event_cnt":2} see /status for details
| shellies/$id/longpush/0               | reports longpush state as 0 (shortpush) or 1 (longpush)
| shellies/$id/overtemperature          | reports 1 when device has overheated, normally 0
| shellies/$id/relay/0/energy           | reports an incrementing energy counter in Watt-minute
| shellies/$id/relay/0/overpower_value  | reports the value in Watts, on which an overpower condition is detected

shelly i4
---------
There is really no i4 for with old API so we get creative here. i4 button
can work in two mode. Either button or switch (configurable via http).
In both cases message will be send on one topic:

**$bid** is button ID (0, 1, 2 or 3). Payload is either on or off

`shellies/$id/input/$bid <on|off>`

Switch
------
Nothing fancy here:

* `shellies/$id/input/0 on` will be sent when button is pressed
* `shellies/$id/input/0 off` will be sent when button is depressed

Good for reading current state of something, like whether pump is currently
working or not.

Button
------
Button works a as toggle switch.

* `shellies/$id/input/0 on` - first button press
* `shellies/$id/input/0 off` - second button press
* `shellies/$id/input/0 on` - third button press

and so on, and so on.

This is usefull when you want to turn on light on, and later off.

Program basically listens for button depresses, and will send on/off\
alternately (by turns).

More
----
More to come. Someday. Just request if you need one supported.

Installing
==========
Program requries:

* [jansson](https://github.com/akheron/jansson) - which probably can be
  installed from your distro repo
* [mosquitto](https://github.com/eclipse/mosquitto) - which you most likely
  already have (just make sure to have also -dev files)
* [embedlog](https://github.com/mlyszczek/embedlog) - which you most likely
  don't have, and need to install by hand from source, but it's easy

```
$ wget http://distfiles.bofc.pl/embedlog/embedlog-0.6.0.tar.gz
$ tar xzf embedlog-0.6.0.tar.gz
$ cd embedlog-0.6.0
$ ./configure
$ make
$ sudo make install
```

Program is using standard autotools, so if you have all deps, just do.
If you are missing something, **configure** step will crash and tell you
what you are missing.

```
$ git clone git://git.bofc.pl/shelldown
$ cd shelldown
$ ./bootstrap.sh
$ ./configure
$ make
$ sudo make install
```
