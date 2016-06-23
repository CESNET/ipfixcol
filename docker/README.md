# <a name="top"></a>IPFIXcol in Docker

IPFIXcol can be easily used in a Docker environment. `Dockerfile` to build an image with IPFIXcol has been prepared. 
Not all plugins and tools are available in the Docker image right now, please raise an issue if you want more functionality in the image.

The Docker image currently serves as a demonstration of IPFIXcol. It does the following:
* receives NetFlow or, preferrably, IPFIX data
* converts input to JSON
* sends JSON data to given output

To build the image just run:

```
docker build -t cesnet:ipfixcol .
```

Then, prepare configuration for the IPFIXcol:
The `$TARGET_IP_ADDRESS` variable must contain a valid IP address of host which should recieve JSON data.
The docker host usually has IP address `172.17.0.1`. 
Be careful to configure a firewall or turn on IP forwarding for the docker cntainer to be able to send the data.

```
sudo mkdir -p /etc/ipfixcol/
sudo cp startup-json.xml /etc/ipfixcol/
sudo sed -i 's/HOST_IP/$TARGET_IP_ADDRESS/g' /etc/ipfixcol/startup-json.xml
```

Run the IPFIXcol container:
```
docker run --rm -v /etc/ipfixcol/:/etc/ipfixcol/ -p 4739:4739/udp -i -t cesnet:ipfixcol -c /etc/ipfixcol/startup-json.xml
```

Now you just need to send IPFIX of NetFlow data to host's `UDP port 4739` and read data on targets `TCP port 4444`.