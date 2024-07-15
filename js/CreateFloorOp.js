class CreateFloorOperator {
    constructor(viewer, owner) {
        this._viewer = viewer;
        this._owner = owner;
        this._vertices = [];
        this._markup;
        this._markupHandle = null;
        this._activeID;
        this._anchorPlane;
        this._midVertices = {};
    }

    _createFloor() {
        let points = [];
        for (let vertex of this._vertices) {
            points.push(vertex.x);
            points.push(vertex.y);
            points.push(vertex.z);
        }

        const faceList = [0, 1, 2, 0, 2, 3];
        let faceCnt = 2;

        for (let key in this._midVertices) {
            let id = Number(key) - 10;
            let prevId = id - 1;
            if (0 == id) {
                prevId = 3;
            }
            points.push(this._midVertices[key].x);
            points.push(this._midVertices[key].y);
            points.push(this._midVertices[key].z);

            faceList.push(prevId);
            faceList.push(points.length / 3 - 1);
            faceList.push(id);

            faceCnt++;
            id++;
        }

        const params = {
            pointCnt: points.length,
            points: points,
            faceCnt: faceCnt,
            faceList: faceList,
        }
        this._owner.invokeCreateFloor(params);
    }

    async Open() {
        this._activeID = -1;
        if (0 == this._vertices.length) {
            const box = await this._viewer.model.getModelBounding(true, false);
            this._vertices.push(box.min);
            this._vertices.push(new Communicator.Point3(box.max.x, box.min.y, box.min.z));
            this._vertices.push(new Communicator.Point3(box.max.x, box.max.y, box.min.z));
            this._vertices.push(new Communicator.Point3(box.min.x, box.max.y, box.min.z));

            this._createFloor();

            this._anchorPlane = Communicator.Plane.createFromPointAndNormal(box.min, new Communicator.Point3(0, 0, 1));
        }

        this._markup = new FloorMarkup(this._viewer, this._vertices);
        this._markupHandle = this._viewer.markupManager.registerMarkup(this._markup);
    }

    Close() {
        if (null != this._markupHandle) {
            this._viewer.markupManager.unregisterMarkup(this._markupHandle);
            this._markupHandle = null;
        }
    }

    onMouseDown(event) {
        if (Communicator.Button.Left != event.getButton()) return;

        var selectPoint = event.getPosition();
        const markup = this._viewer.markupManager.pickMarkupItem(selectPoint);
        if (markup) {
            this._activeID = markup.GetSelectedId();
            if (0 <= this._activeID) {
                event.setHandled(true);
            }
        }
    }

    onMouseMove(event) {
        if (0 <= this._activeID) {
            const raycast = this._viewer.view.raycastFromPoint(event.getPosition());
            const vertex = Communicator.Point3.zero();
            if (this._anchorPlane.intersectsRay(raycast, vertex)) {
                if (10 > this._activeID) {
                    this._vertices[this._activeID] = vertex;
                }
                else {
                    this._midVertices[this._activeID] = vertex;
                }
                this._markup.SetVertex(this._activeID, vertex);
            }
            event.setHandled(true);
        }
    }

    onMouseUp(event) {
        if (0 <= this._activeID) {
            this._createFloor();
            event.setHandled(true);
        }
        this._activeID = -1;
    }
}

class FloorMarkup extends Communicator.Markup.MarkupItem {
    constructor(viewer, vertices) {
        super();
        this._viewer = viewer;
        this._vertices = vertices;
        this._cornerCirArr = [];
        this._middleCirArr = [];
        this._rad = 4;
        this._selectedId = -1;
        this._lines = new Communicator.Markup.Shape.LineCollection();
        this._lines.setStrokeColor(new Communicator.Color(255, 0, 0));
        this._lines.setStrokeWidth(1);
        this._midVertices = {};

        for (let i = 0; i < vertices.length; i++) {
            const cir = new Communicator.Markup.Shape.Circle();
            cir.setRadius(this._rad);
            cir.setFillOpacity(0);
            cir.setStrokeColor(new Communicator.Color(255, 0, 0));
            cir.setStrokeWidth(2);
            this._cornerCirArr.push(cir);
        }

        for (let i = 0; i < vertices.length; i++) {
            const cir = new Communicator.Markup.Shape.Circle();
            cir.setRadius(this._rad);
            cir.setFillOpacity(0);
            cir.setStrokeColor(new Communicator.Color(255, 255, 0));
            cir.setStrokeWidth(2);
            this._middleCirArr.push(cir);
        }
    }

    draw() {
        const renderer = this._viewer.markupManager.getRenderer();
        this._lines.clear();
        for (let i = 0; i < this._vertices.length; i++) {
            const pnt3d = this._viewer.view.projectPoint(this._vertices[i]);
            const pnt2d = Communicator.Point2.fromPoint3(pnt3d);
            this._cornerCirArr[i].setCenter(pnt2d);

            renderer.drawCircle(this._cornerCirArr[i]);

            let prevId = i - 1;
            if (0 == i) {
                prevId = this._vertices.length - 1;
            }
            const prev_pnt3d = this._viewer.view.projectPoint(this._vertices[prevId]);
            const prev_pnt2d = Communicator.Point2.fromPoint3(prev_pnt3d);

            let midId = i + 10;
            const vertex = this._midVertices[String(midId)];
            if (undefined == vertex) {
                const stPnt3d = this._viewer.view.projectPoint(this._vertices[prevId]);
                const enPnt3d = this._viewer.view.projectPoint(this._vertices[i]);
                const stPnt2d = Communicator.Point2.fromPoint3(stPnt3d);
                const enPnt2d = Communicator.Point2.fromPoint3(enPnt3d);
                const mid2d = stPnt2d.add(enPnt2d.subtract(stPnt2d).scale(0.5));
                this._middleCirArr[i].setCenter(mid2d);

                this._lines.addLine(prev_pnt2d, pnt2d)
            }
            else {
                const mid3d = this._viewer.view.projectPoint(vertex);
                const mid2d = Communicator.Point2.fromPoint3(mid3d);
                this._middleCirArr[i].setCenter(mid2d);

                this._lines.addLine(prev_pnt2d, mid2d);
                this._lines.addLine(mid2d, pnt2d);
            }
            renderer.drawCircle(this._middleCirArr[i]);
        }
        renderer.drawLines(this._lines);
    }

    hit(point) {
        for (let i = 0; i < this._cornerCirArr.length; i++) {
            const cent = this._cornerCirArr[i].getCenter();
            const dist = Communicator.Point2.distance(cent, point);
            if ((this._rad + 1) * 2 >= dist){
                this._selectedId = i;
                return true;
            }

            // Middle circle hit
            let prevId = i - 1;
            if (0 == i) {
                prevId = this._vertices.length - 1;
            }
            const stPnt2d = this._cornerCirArr[prevId].getCenter();
            const enPnt2d = this._cornerCirArr[i].getCenter();
            const mid2d = stPnt2d.add(enPnt2d.subtract(stPnt2d).scale(0.5));
            const midDist = Communicator.Point2.distance(mid2d, point);
            if ((this._rad + 1) * 2 >= midDist){
                this._selectedId = i + 10;
                console.log(this._selectedId);
                return true;
            }
        }
        this._selectedId = -1;
        return false
    }

    remove() {
        return;
    }

    GetSelectedId() {
        return this._selectedId;
    }

    SetVertex(id, vertex) {
        if (10 > id) {
            this._vertices[id] = vertex;
        }
        else {
            this._midVertices[id] = vertex;
        }
        this._viewer.markupManager.refreshMarkup();
    }
}