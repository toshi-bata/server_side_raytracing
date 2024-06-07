class Main {
    constructor() {
        this._viewer;
        this._port;
        this._reverseProxy;
        this._sessionId;
        this._serverCaller;
        this._tol = 1e-4;
        this._units = {
            "1.0": "mm",
            "25.4": "inch",
            "10.0": "cm",
            "1000.0": "m"
        };
    }

    start (port, reverseProxy) {
        this._port = port;
        this._reverseProxy = reverseProxy;
        this._requestServerProcess();
        this._createViewer();
        this._initEvents();
    }

    _requestServerProcess () {
        this._sessionId = create_UUID();

        // Create PsServer caller
        let psServerURL = window.location.protocol + "//" + window.location.hostname + ":" + this._port;
        if (this._reverseProxy) {
            psServerURL = window.location.protocol + "//" + window.location.hostname + "/httpproxy/" + this._port;
        }

        this._serverCaller = new ServerCaller(psServerURL, this._sessionId);

    }

    _createViewer() {
        let endpoint = "ws://" + window.location.hostname + ":11182";
        if (this._reverseProxy) {
            if ("http:" == window.location.protocol) {
                endpoint = "ws://" + window.location.hostname + "/wsproxy/11182";
            }
            else {
                endpoint = "wss://" + window.location.hostname + "/wsproxy/11182";
            }
        }
        this._viewer = new Communicator.WebViewer({
            containerId: "container",
            model: "_empty",
            endpointUri: endpoint,
            rendererType: Communicator.RendererType.Client
        });

        this._viewer.setCallbacks({
            sceneReady: () => {
                this._viewer.view.setBackgroundColor(new Communicator.Color(255, 255, 255), new Communicator.Color(230, 204, 179));
            },
            timeout: () => {
                this._serverCaller.CallServerPost("Clear");
                console.log("Timeout");
            }
        });

        this._viewer.start();
    }

    _initEvents() {
        let resizeTimer;
        let interval = Math.floor(1000 / 60 * 10);
        $(window).resize(() => {
        if (resizeTimer !== false) {
            clearTimeout(resizeTimer);
        }
        resizeTimer = setTimeout(() => {
            this._viewer.resizeCanvas();
        }, interval);
        });

        // Before page reload or close
        $(window).on('beforeunload', (e) => {
            this._serverCaller.CallServerPost("Clear");
        });

        // File uploading
        $("#UploadDlg").dialog({
            autoOpen: false,
            height: 380,
            width: 450,
            modal: true,
            title: "CAD file upload",
            closeOnEscape: true,
            position: {my: "center", at: "center", of: window},
            buttons: {
                'Cancel': () => {
                    $("#UploadDlg").dialog('close');
                }
            }
        });

        $("#UploadDlg").submit((e) => {
            // Cancel default behavior (abort form action)
            e.preventDefault();

            if (0 == e.target[0].files.length) {
                alert("Please select a file.");
                return;
            }

            // Get file name
            // let scModelName = e.target[0].files[0].name;
            // scModelName = scModelName.split(".").shift();
            const scModelName = "model";

            // Get options
            const entityIds = Number($("#checkEntityIds").prop('checked'));
            const sewModel = Number($("#checkSewModel").prop('checked'));
            const sewingTol = Number($("#sewingTol").val());

            const params = {
                entityIds: entityIds,
                sewModel: sewModel,
                sewingTol: sewingTol
            }

            const formData = new FormData(e.target);

            this._loadModel(params, formData, scModelName);

            $('#UploadDlg').dialog('close');
        });

        // Tolerance select
        $('#sewingTol').val(0.01);
        $('#classifyTol').val(0.0001);

        // Simple command
        $(".toolbarBtn").on("click", (e) => {
            let command = $(e.currentTarget).data("command");
            let isOn = $(e.currentTarget).data("on");

            switch(command) {
                case "Upload": {
                    $('#UploadDlg').dialog('open');
                } break;
                case "Raytracing": {
                    const params = {};
                    this._serverCaller.CallServerPost(command, params).then(() => {

                    });
                } break;
                default: {} break;
            }
        });
    }

    _loadModel(params, formData, scModelName) {
        $("#loadingImage").show();
        this._serverCaller.CallServerPost("Clear").then(() => {
            this._requestServerProcess();
            this._serverCaller.CallServerPost("SetOptions", params).then(() => {
                this._serverCaller.CallServerSubmitFile(formData).then((arr) => {
                    let dataArr = Array.from(arr);
                    let unit = dataArr.shift();
                    unit = unit.toFixed(1);
                    unit = this._units[unit];
                    const area = dataArr.shift();
                    const volume = dataArr.shift();
                    const COG = [dataArr.shift(), dataArr.shift(), dataArr.shift()];
                    const BB = [dataArr.shift(), dataArr.shift(), dataArr.shift()];

                    const partProps = '<ul><li>Unit: ' + unit + '</li>'
                    + '<li>Surface area: ' + area.toFixed(2) + '<br>'
                    + '<li>Volume: ' + volume.toFixed(2) + '</li>'
                    + '<li>Center of Gravity: X= ' + COG[0].toFixed(2) + ', Y= ' + COG[1].toFixed(2) + ', Z= ' + COG[2].toFixed(2) + '</li>'
                    + '<li>Bounding box: X= ' + BB[0].toFixed(2) + ', Y= ' + BB[1].toFixed(2) + ', Z= ' + BB[2].toFixed(2) + '</li></ul>'

                    $("#dataInfo").html(partProps);

                    this._viewer.model.switchToModel(this._sessionId + "/" + scModelName).then((nodes) => {
                        $("#loadingImage").hide();
                    });
                });
            });
        });
    }
}
