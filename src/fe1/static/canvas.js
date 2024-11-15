const stickyR = 22
const panelW = 480

function loadXY(key, x, y) {
    return JSON.parse(localStorage.getItem(key) ||
        `{"x":${x || 0},"y":${y || 0}}`)
}

function saveXY(key, x, y) {
    localStorage.setItem(key, JSON.stringify({ x, y }))
}

function setNodePos(plane, x, y) {
    plane.setAttribute('transform', `translate(${x}, ${y})`)
}

function draw() {
    perf()

    ctx.xy = new Map
    ctx.linesFrom = new Map
    ctx.linesTo = new Map

    const { mode, xy, linesFrom, linesTo } = ctx
    const errors = ctx.errors.get(mode)
    const warnings = ctx.warnings.get(mode)
    const ents = ctx.ents[mode]
    const plane = $('#plane')

    const { x, y } = loadXY(`${mode}:plane:xy`)
    setNodePos(plane, x, y)

    const centerX = (w.innerWidth - panelW) / 2 - x
    const centerY = w.innerHeight / 2 - y

    const lines = new Map
    const circles = []

    for (const id in ents) {
        const fullId = `${mode}:${id}`
        const xyKey = `xy:${fullId}`
        const text = id.length <= 32
            ? id
            : id.substring(0, 29) + '...'

        !localStorage.getItem(xyKey)
            && saveXY(xyKey, centerX, centerY)

        const { x, y } = loadXY(xyKey)
        xy.set(id, { x, y })

        const className = errors.has(id) ? 'error'
            : warnings.has(id) ? 'warning'
            : mode

        circles.push(`<g id="${esc(fullId)}" transform="translate(${x}, ${y})">
            <circle r="8" class="${className}"></circle>
            <text x="14" y="5">${esc(text.trim() || '* empty')}</text>
        </g>`)
    }

    const makeLines = mode === 'class'
        ? makeClassLines
        : makeObjectLines

    for (const id of xy.keys()) {
        makeLines(lines, id)
    }

    plane.innerHTML = Array.from(lines.values()).join('') + circles.join('')

    $$('g > circle').forEach(circle => {
        circle.addEventListener('mousedown', startDraggingCircle)
    })

    $$('g > text').forEach(text => {
        text.addEventListener('click', clickOnCircle)
    })

    $$('line').forEach(line => {
        const [fromId, toId] = line.id.split(/:(.*)/)

        const from = linesFrom.get(fromId) || []
        const to = linesTo.get(toId) || []

        from.push(line)
        to.push(line)

        from.length === 1 && linesFrom.set(fromId, from)
        to.length === 1 && linesTo.set(toId, to)
    })

    perf('draw')
}

function makeClassLines(lines, fromId) {
    const { xy, ents: { class: classes } } = ctx
    const from = classes[fromId]
    const { x: x1, y: y1 } = xy.get(fromId)

    for (const key in from) {
        if (key === 'id') continue

        const toId = from[key]?.class
        if (!classes[toId]) continue

        const lineId = `${fromId}:${toId}`
        const mirrorId = `${toId}:${fromId}`
        if (lines.has(lineId) || lines.has(mirrorId)) continue

        const { x: x2, y: y2 } = xy.get(toId)
        lines.set(lineId, lineHTML(lineId, x1, y1, x2, y2))
    }
}

function makeObjectLines(lines, fromId) {
    const { xy, ents: { object: objects, class: classes } } = ctx
    const from = objects[fromId]
    const fromClass = classes[from.class]
    const errors = ctx.errors.get('object').get(fromId)
    const { x: x1, y: y1 } = xy.get(fromId)

    for (const key in from) {
        if (key === 'id') continue
        if (key === 'class') continue

        const type = fromClass?.[key]?.type
        const isRel = type === 'rel'
        const isMultiRel = type === 'rels'

        if (errors?.get(key) && !isMultiRel) continue
        let toIds = isRel ? [from[key]] : []

        if (isMultiRel && from[key]) {
            try {
                toIds = JSON.parse(from[key])
                if (!Array.isArray(toIds)) {
                    toIds = []
                }
            } catch {}
        }

        toIds?.forEach(toId => {
            if (!objects[toId]) return

            const lineId = `${fromId}:${toId}`
            const mirrorId = `${toId}:${fromId}`
            if (lines.has(lineId) || lines.has(mirrorId)) return

            const { x: x2, y: y2 } = xy.get(toId)
            lines.set(lineId, lineHTML(lineId, x1, y1, x2, y2))
        })
    }
}

function lineHTML(id, x1, y1, x2, y2) {
    return `<line
        id="${esc(id)}"
        x1="${x1}" y1="${y1}"
        x2="${x2}" y2="${y2}"
    ></line>`
}

function clickOnCircle(e) {
    e.stopPropagation()
    location.hash = e.target.parentNode.id
}

function startDraggingCircle(e) {
    e.stopPropagation()

    const { mode, xy, linesFrom, linesTo } = ctx
    const circles = []

    const g = e.target.parentNode
    const id = g.id.split(/:(.*)/)[1]
    const { x, y } = xy.get(id)

    circles.push({
        id, g, x, y,
        linesFrom: linesFrom.get(id),
        linesTo: linesTo.get(id),
    })

    const canStickY = new Set

    for (let i = 1; i <= 10; ++i) {
        canStickY.add(y + stickyR * i)
    }

    for (const [sibId, { x: sibX, y: sibY }] of xy) {
        if (sibId === id) continue

        if (sibX === x && sibY > y && canStickY.has(sibY)) {
            const g = d.getElementById(`${mode}:${sibId}`)
            const { x, y } = xy.get(sibId)

            circles.push({
                id: sibId, g, x, y,
                linesFrom: linesFrom.get(sibId),
                linesTo: linesTo.get(sibId),
            })
        }
    }

    ctx.moving = {
        circles,
        cX: e.clientX,
        cY: e.clientY,
    }
}

function startDraggingPlane(e) {
    if (e.target.id !== 'canvas') return
    e.stopPropagation()

    const { x, y } = loadXY(`${ctx.mode}:plane:xy`)

    ctx.movingPlane = {
        plane: $('#plane'),
        x, y,
        cX: e.clientX,
        cY: e.clientY,
    }
}

function moveCircle(g, x, y, linesFrom, linesTo) {
    setNodePos(g, x, y)

    linesFrom?.forEach(line => {
        line.setAttribute('x1', x)
        line.setAttribute('y1', y)
    })

    linesTo?.forEach(line => {
        line.setAttribute('x2', x)
        line.setAttribute('y2', y)
    })
}

function onMouseMove(e) {
    const { moving, movingPlane } = ctx

    if (movingPlane) {
        let { plane, x, y, cX, cY } = movingPlane

        x += e.clientX - cX
        y += e.clientY - cY

        setNodePos(plane, x, y)
    } else if (moving) {
        const { circles, cX, cY } = moving

        circles.forEach(({ g, x, y, linesFrom, linesTo }) => {
            x += e.clientX - cX
            y += e.clientY - cY

            moveCircle(g, x, y, linesFrom, linesTo)
        })
    }
}

function onMouseUp(e) {
    onMouseMove(e)

    const { moving, movingPlane } = ctx
    const { mode, xy } = ctx

    if (movingPlane) {
        let { x, y, cX, cY } = movingPlane

        x += e.clientX - cX
        y += e.clientY - cY

        saveXY(`${ctx.mode}:plane:xy`, x, y)
        ctx.movingPlane = null
    } else if (moving) {
        const { circles, cX, cY } = moving
        const isSingle = circles.length === 1

        circles.forEach(({ id, g, x, y, linesFrom, linesTo }) => {
            x += e.clientX - cX
            y += e.clientY - cY

            if (isSingle) {
                for (const [sibId, { x: sibX, y: sibY }] of xy) {
                    if (sibId === id) continue

                    const dx = Math.abs(x - sibX)
                    const dy = y - sibY

                    if (dx <= stickyR && dy <= stickyR && dy > 0) {
                        x = sibX
                        y = y > sibY
                            ? sibY + stickyR
                            : sibY - stickyR

                        moveCircle(g, x, y, linesFrom, linesTo)
                    }
                }
            }

            xy.set(id, { x, y })
            saveXY(`xy:${mode}:${id}`, x, y)
        })

        ctx.moving = null
    }
}
