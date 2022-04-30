shelly 1pm
==========
implemented
-----------
| api                              | description |
| -------------------------------- | ----------- |
| shellies/<id>/relay/0            | to report status: on, off or overpower (the latter only for Shelly1PM)
| shellies/<id>/relay/0/power      | reports instantaneous power in Watts
| shellies/<id>/temperature        | reports internal device temperature in °C
| shellies/<id>/temperature_f      | reports internal device temperature in °F
| shellies/<id>/temperature_status | reports Normal, High, Very High

implemented (but not existing in gen1)
--------------------------------------
| api                            | description |
| ------------------------------ | ----------- |
| shellies/<id>/relay/0/voltage  | last measured voltage in Volts

not implemented
---------------
| api                                   | description |
| ------------------------------------- | ----------- |
| shellies/<id>/input/0                 | reports the state of the SW terminal
| shellies/<id>/input_event/0           | reports input event and event counter, e.g.: {"event":"S","event_cnt":2} see /status for details
| shellies/<id>/longpush/0              | reports longpush state as 0 (shortpush) or 1 (longpush)
| shellies/<id>/overtemperature         | reports 1 when device has overheated, normally 0
| shellies/<id>/relay/0/command         | accepts on, off or toggle and applies accordingly
| shellies/<id>/relay/0/energy          | reports an incrementing energy counter in Watt-minute
| shellies/<id>/relay/0/overpower_value | reports the value in Watts, on which an overpower condition is detected
