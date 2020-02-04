
[Red Pitaya documentation](https://redpitaya.readthedocs.io/en/latest/index.html)

[Red Pitaya code repository](https://github.com/RedPitaya/RedPitaya)


# Connect to Red Pitaya
## Connect via debug console
Using the screen command:

    screen /dev/ttyUSB0 115200

Exit screen using `Ctrl+a d`.

The serial-over-USB console is available at all times when the USB
cable is connected, even if Red Pitaya hasn't booted yet.  You can
watch the boot process by connecting the USB communication first and
then the USB power cable.


## Connect via ssh
User and password both 'root':

    ssh root@169.254.87.41
    
or with (trivial) password on command line:

    sshpass -p 'root' ssh root@169.254.87.41


## Connect via telnet

To use SCPI over telnet first start SCPI server on debug console:

    root@rp-f05dcb:~# systemctl start redpitaya_scpi

Then use telnet with correct IP:

    # telnet
    telnet> toggle crlf
    Will send carriage returns as telnet <CR><LF>.
    telnet> open 169.254.70.102 5000
    Trying 169.254.70.102...
    Connected to 169.254.70.102.
    Escape character is '^]'.
    DIG:PIN LED1,1
    DIG:PIN LED2,1
    DIG:PIN LED0,1
    DIG:PIN LED1,0
    ^]
    telnet> 
    Connection closed.
