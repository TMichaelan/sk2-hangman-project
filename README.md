# sk2-hangman-project

# Launch

## Client:
```shell
pip install -r requirements.txt
cd client
python main.py
```

## Server:

### Windows: 

1. Install docker
2. Type the following commands
```shell
docker build -t server .
docker run -p 4000:4000 server
```


### Linux:
```shell
make
cd server
./upserver
```

### Port/IP:

Default port is 4000. To change the port follow the next steps:

1. Change "port" variable on port you want in <b>server.h</b>
2. Change "port" in python config file. 
3. If you use docker,just change the <b>-p</b> flag to the desired port (e.x.: docker run -p 8000:8000 server)

If you raise the server on the docker, you can connect to it from different devices on the same network.  
Change the ip in the python configuration file to the ip of the device from which you start the server.

Find out your IP:

Windows: ipconfig

Linux: ifconfig
