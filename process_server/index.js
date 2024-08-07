const serverPORT = 8080;    // Port number of this server
const startPort = 8888;     // Start port number of ExLuServer
const processCnt = 10;      // Max count of ExLuServer instance
const execPath = '..\\win64\\ExLuServer.exe';
let processMap = {};

const http = require('http');
const server = http.createServer(function(req, res) {
    res.setHeader('Access-Control-Allow-Headers','Origin, X-Requested-With, Content-Type, Authorization, Accept');
    res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
    res.setHeader('Access-Control-Allow-Origin', '*');

    const createProcessInstance = (port) => {
        const exec = require('child_process').exec;
        let cp = exec(execPath + ' ' + port, (err, stdout, stderr) => {
            if (stdout) console.log('stdout', stdout);
            if (stderr) console.log('stderr', stderr);
            if (err !== null) console.log('err', err);
        });

        const ppid = cp.pid;

        // Get child PID
        const psTree = require('ps-tree');
        psTree(ppid, (err, children) => {
            if (err) {
                console.error(err)
                return
            }

            let ret = {port: 0, pid: 0};
            if (1 == children.length) {
                const pid = Number(children[0].PID);

                processMap[port] = {
                    ppid: ppid,
                    pid: pid,
                    time: new Date().getTime()
                }

                console.log('  ExLuServer was started');
                console.log('    PORT: ' + String(port));
                console.log('    PPID: ' + ppid);
                console.log('    PID:  ' + pid);

                ret.port =  port,
                ret.pid = pid
            }

            res.writeHead(200, {"Content-Type": "application/json"});
            res.end(JSON.stringify(ret));
        });
    }

    const killProcessInstance = (port, pid) => {
        try {
            process.kill(pid)

            console.log('  ExLuServer was killed');
            console.log('    PORT: ' + String(port));
            console.log('    PID:  ' + String(pid));

            processMap[port] = undefined;
        } catch (e) {
            console.log(e);
        }
    }

    if ('POST' == req.method) {
        const url = req.url;
        switch (url) {
            case '/start': {
                // Find unused port
                let port = 0;
                for (let i = startPort; i < startPort + processCnt; i++) {
                    if (undefined == processMap[i]) {
                        port = i;
                        break;
                    }
                }

                // Execute server if an open port is there
                if (0 < port) {
                    createProcessInstance(port);
                }
                else {
                    // Kill the oldest port instance
                    let pids = [];
                    const now = new Date().getTime();
                    let oldest = now;
                    for (let key in processMap) {
                        const data = processMap[key];
                        const time = data.time;
                        const deff = now - time;
                        if (30 * 60 * 1000 < deff) {
                            if (oldest > time) {
                                oldest = time;
                                port = Number(key);
                            }
                        }
                    }
                    if (0 < port) {
                        const data = processMap[port];
                        const pid = data.pid; 

                        killProcessInstance(port, pid);

                        // Wait for a while
                        setTimeout(() => {
                            createProcessInstance(port);
                        }, 3000);
                    }
                    else {
                        const ret = {port: 0, pid: 0}
                        res.writeHead(200, {"Content-Type": "application/json"});
                        res.end(JSON.stringify(ret));
                    }
                }
            } break;
            case '/end': {
                req.on('data', (chunk) => {
                    let data = '';
                    data += chunk;
                    const paramArr = data.split('&');
                    const portArr = paramArr[0].split('=');
                    const pidArr = paramArr[1].split('=');
                    if ('port' == portArr[0]) {
                        const killPort = Number(portArr[1]);
                        const killPid = Number(pidArr[1]);
                        const data = processMap[killPort];
                        if (undefined != data) {
                            if (killPid == data.pid) {
                                processMap[killPort] = undefined;
                                console.log('  ExLuServer was terminated');
                                console.log('    PORT: ' + String(killPort));
                                console.log('    PID:  ' + String(killPid));
                            }
                        }
                    }
                })
                .on('end', () => {
                    res.writeHead(200, {"Content-Type": "application/json"});
                    res.end("success");
                })
            } break;
            default: break;
        }
    }
}).listen(serverPORT, () => {
    console.log('Process server listening on port ' + serverPORT + '...');
});

