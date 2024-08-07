class ServerCaller {
    constructor(serverURL) {
        this._processServerURL = serverURL;
        this._exServerURL;
        this._sessionId;
    }

    SetNewSession(serverURL, sessionId) {
        this._exServerURL = serverURL;
        this._sessionId = sessionId;
    }

    CallServerPost(command, params = {}, retType = null) {
        return new Promise((resolve, reject) => {
            if (undefined == this._exServerURL || undefined == this._sessionId) reject;

            // Add session ID in params
            const encParams = encodeHTMLForm(params);

            let xhr = new XMLHttpRequest();
            xhr.open("POST", this._exServerURL + "/" + command + "?session_id=" + this._sessionId, true);
            xhr.setRequestHeader( 'Content-Type', 'application/x-www-form-urlencoded' );
            if ("INT" == retType || "FLOAT" == retType) {
                xhr.responseType = "arraybuffer";
            }
            xhr.onreadystatechange = () => {
                if(xhr.readyState === XMLHttpRequest.DONE && xhr.status === 200) {
                    console.log(xhr.statusText);
                    switch (retType) {
                        case "INT": {
                            let arr = new Int32Array(xhr.response);
                            if (arr.length) return resolve(arr);
                            else reject();
                        } break;
                        case "FLOAT": {
                            const arr = new Float32Array(xhr.response); 
                            if (arr.length) return resolve(arr);
                            else reject();
                        } break;
                        default: {
                            if ("OK" == xhr.statusText) return resolve(xhr.response);
                            else return reject();
                        } break;
                    }
                }
            }
            xhr.onerror = () => {
                return reject(xhr.statusText);
            };

            xhr.send(encParams);
        });
    }

    CallServerSubmitFile(formData) {
        return new Promise((resolve, reject) => {
            if (undefined == this._exServerURL || undefined == this._sessionId) reject;
            
            const xhr = new XMLHttpRequest();
            xhr.responseType = "arraybuffer";

            xhr.onload = (e) => {
                console.log(xhr.statusText);
                const arr = new Float32Array(xhr.response); 
                if (arr.length) resolve(arr);
                else reject();
            };

            xhr.open("POST" , this._exServerURL + "/FileUpload?session_id=" + this._sessionId);

            xhr.send(formData);
        });
    }

    CallProcessServer(request, params = {}) {
        return new Promise((resolve, reject) => {
            const encParams = encodeHTMLForm(params);

            var xhr = new XMLHttpRequest();
            xhr.onreadystatechange = function () {
                if (xhr.readyState === XMLHttpRequest.DONE && xhr.status === 200) {
                    return resolve(xhr.response);
                }
            };
            
            xhr.open("POST", this._processServerURL + "/" + request);
            xhr.setRequestHeader( 'Content-Type', 'application/x-www-form-urlencoded' );
            
            xhr.send(encParams);
        });
    }
}