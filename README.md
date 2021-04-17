NetTalk is free, end-to-end encrypted VoIP + Chat
-------------------------------------------------

![alt text](https://raw.githubusercontent.com/ecnx/nettalk/main/screenshot.png)

It's quite easy to setup your own NetTalk server.  
You will need a VPS or an OpenWRT router with IPv4.  
Proxy server will only bind **TWO** devices from different Local Area Networks,  
but does not keep user credentials as it's end-to-end encrypted.  

Then you could run NetTalk client.  
You will need to provide password to decrypt the config.  
Config contains server name, opposite user certificate and your key.  

When both users are online, they can communicate  
with each other via sound or by text chat.  

To create configs for both users:  
```
./aux/gen conf <server-addr> <server-port>
```
Note: fxcrypt, another project here, is needed to encrypt config  

Launch NetTalk client:  
```
./bin/nettalk conf/a.conf or ./bin/nettalk conf/b.conf
```

To setup NetTalk proxy server:  
```
./nettalk-proxy <server-addr> <server-port>
```
Note: nettalk-proxy, another project here, is needed to make it work  

NetTalk uses following libraries / algorithms:  
* mbedtls (RSA, AES-GCM, HMAC)
* soxr (resampling)
* opencoreamr-nb (voice compression)
* libevent (notifications)
* gtk3 (user interface)
 
 How to connect with proxy-server via SOCKS-5 proxy, e.g. via Tor?  
 ```
 ./nettalk --socks5h 127.0.0.1:9050 conf/test.conf
 ```
