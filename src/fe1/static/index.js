const w = window
const d = document
const $ = d.querySelector.bind(d)
const $$ = d.querySelectorAll.bind(d)

const ctx = {
    type: localStorage.getItem('type') || 'class',
}

d.addEventListener('DOMContentLoaded', () => {
    fetch('/entities').then(r => r.json()).then(r => {
        if (r?.error) throw Error(r?.error)

        ['objects', 'classes'].forEach(type => {
            const ents = r[type]

            Object.keys(ents).forEach(id => {
                ents[id].id = id
            })
        })

        ctx.ents = {
            object: r.objects,
            class: r.classes,
        }

        ctx.classKeyTypes = r.class_key_types

        draw()
        showEntity(location.hash.substring(1))
    }).catch(openError)

    $('#type').value = ctx.type

    $('#type').addEventListener('change', () => {
        ctx.type = $('#type').value
        localStorage.setItem('type', ctx.type)
        draw()
    })

    $('#add').addEventListener('click', openAdd)
    $('#canvas').addEventListener('mousedown', onPlaneDragStart)

    $('#panel .popup-close').addEventListener('click', () => {
        location.hash = ''
    })

    $('#popup .popup-close').addEventListener('click', closePopup)
    $('#popup-shadow').addEventListener('click', closePopup)

    w.addEventListener('mousemove', onMouseMove)
    w.addEventListener('mouseup', onMouseUp)
    w.addEventListener('hashchange',
        () => showEntity(location.hash.substring(1)))

    w.onerror = (msg, url, line, col, error) =>
        openError(`${msg}<br>${url}:${line}:${col}`)
})

function addEnt(type, entity) {
    onSubmitStart('adding')

    fetch(`/${type}`, {
        method: 'PUT',
        body: JSON.stringify(entity)
    }).then(r => r.json()).then(r => {
        if (r?.error) throw Error(r?.error)
        onSubmitSuccess()
        const entity = r[type]
        ctx.ents[type][entity.id] = entity
        draw()
        location.hash = ''
        location.hash = `${type}:${entity.id}`
    }).catch(onSubmitError)
}

function delEnt(nodeId) {
    const [type, id] = nodeId.split(':')

    onSubmitStart('deleting')

    fetch(`/${type}`, {
        method: 'DELETE',
        body: JSON.stringify({ id }),
    }).then(r => r.json()).then(r => {
        if (r?.error) throw Error(r?.error)
        onSubmitSuccess()
        delete ctx.ents[type][id]
        draw()
        location.hash = ''
    }).catch(onSubmitError)
}

function setEntKey(nodeId, key, value, actionName) {
    const [type, id] = nodeId.split(':')

    onSubmitStart(actionName)

    fetch(`/${type}/key`, {
        method: 'PUT',
        body: JSON.stringify({ id, key, value })
    }).then(r => r.json()).then(r => {
        if (r?.error) throw Error(r?.error)
        onSubmitSuccess()
        ctx.ents[type][id][key] = value
        draw()
        location.hash = ''
        location.hash = nodeId
    }).catch(onSubmitError)
}

function delEntKey(nodeId, key) {
    const [type, id] = nodeId.split(':')

    onSubmitStart('deleting')

    fetch(`/${type}/key`, {
        method: 'DELETE',
        body: JSON.stringify({ id, key }),
    }).then(r => r.json()).then(r => {
        if (r?.error) throw Error(r?.error)
        onSubmitSuccess()
        delete ctx.ents[type][id][key]
        draw()
        location.hash = ''
        location.hash = nodeId
    }).catch(onSubmitError)
}

function draw() {
    ctx.xy = {}
    ctx.errors = {}
    ctx.linesFrom = {}
    ctx.linesTo = {}
    ctx.linesIds = {}

    const canvas = $('#canvas')
    const plane = $('#plane')

    const fullW = w.innerWidth
    const fullH = w.innerHeight
    const centerX = Math.round(fullW / 2)
    const centerY = Math.round(fullH / 2)

    canvas.setAttribute('width', fullW)
    canvas.setAttribute('height', fullH)

    const { type, linesFrom, linesTo } = ctx
    const ents = ctx.ents[type]

    let { x = 0, y = 0 } =
        JSON.parse(localStorage.getItem(type + ':plane:xy') || '{}')

    plane.setAttribute('transform', `translate(${x}, ${y})`)

    let linesHTML = ''
    let nodesHTML = ''

    Object.keys(ents).forEach(id => {
        const gId = type + ':' + id
        const xyKey = 'xy:' + gId
        const text = id.substring(0, 32) + (id.length > 32 ? '...' : '')

        const errors = type === 'class'
            ? checkClass(id)
            : checkObject(id)

        const classes = [type]

        if (Object.keys(errors).length > 0) {
            ctx.errors[gId] = errors
            classes.push('error')
        }

        let { x, y } = JSON.parse(localStorage.getItem(xyKey) || '{}')

        if (x === undefined || y === undefined) {
            x = centerX
            y = centerY
            localStorage.setItem(xyKey, JSON.stringify({ x, y }))
        }

        ctx.xy[id] = { x, y }

        nodesHTML += `<g id="${gId}" transform="translate(${x}, ${y})">
            <circle r="8" class="${classes.join(' ')}"></circle>
            <text x="13" y="5">${text || 'undefined'}</text>
        </g>`
    })

    const makeLines = type == 'class' ? makeClassLines : makeObjLines

    Object.keys(ents).forEach(id => {
        const html = makeLines(id)

        if (html) {
            linesHTML += html
        }
    })

    plane.innerHTML = linesHTML + nodesHTML
    ctx.linesIds = {} // free some mem

    $$('g > circle').forEach(circle => {
        circle.addEventListener('mousedown', startMoving)
    })

    $$('g > text').forEach(text => {
        text.addEventListener('click', clickEntity)
    })

    $$('line').forEach(line => {
        const [from, to] = line.id.split(':')

        if (!linesFrom[from]) linesFrom[from] = []
        if (!linesTo[to]) linesTo[to] = []

        linesFrom[from].push(line)
        linesTo[to].push(line)
    })
}

function makeClassLines(fromId) {
    let html = ''

    const { xy, linesIds } = ctx
    const from = ctx.ents[ctx.type][fromId]
    const { x: x1, y: y1 } = xy[fromId]

    Object.keys(from).forEach(key => {
        if (key === 'id') { return }

        const [prefix, name] = key.split(':')

        if (prefix === 'rel' && name) {
            const toId = from[key]
            if (!xy[toId]) { return }

            const lineId = fromId + ':' + toId
            const { x: x2, y: y2 } = xy[toId]

            if (linesIds[lineId]) { return }

            html += line(lineId, x1, y1, x2, y2)

            linesIds[lineId] = true
            linesIds[toId + ':' + fromId] = true
        }
    })

    return html
}

function makeObjLines(fromId) {
    let html = ''

    const { xy, linesIds, ents } = ctx
    const { x: x1, y: y1 } = xy[fromId]
    const objs = ents[ctx.type]
    const from = objs[fromId]
    const fromClass = ents.class[from.class]

    if (!fromClass) { return }

    Object.keys(from).forEach(key => {
        if (key === 'id' || key === 'class') { return }
        if (!fromClass['rel:' + key]) { return }

        const toId = from[key]
        if (!xy[toId]) { return }

        const lineId = fromId + ':' + toId
        const { x: x2, y: y2 } = xy[toId]

        if (linesIds[lineId]) { return }

        html += line(lineId, x1, y1, x2, y2)

        linesIds[lineId] = true
        linesIds[toId + ':' + fromId] = true
    })

    return html
}

function line(id, x1, y1, x2, y2) {
    return `<line id="${id}" x1="${x1}" y1="${y1}" x2="${x2}" y2="${y2}">
    </line>`
}

function startMoving(e) {
    e.stopPropagation()

    const g = e.target.parentNode
    const gId = g.id
    const entId = gId.split(':')[1]
    const xyKey = 'xy:' + gId

    let { x, y } = JSON.parse(localStorage.getItem(xyKey))

    ctx.moving = {
        g, x, y,
        cX: e.clientX,
        cY: e.clientY,
        linesFrom: ctx.linesFrom[entId],
        linesTo: ctx.linesTo[entId],
    }
}

function onMouseMove(e) {
    const { moving, movingPlane } = ctx

    if (moving) {
        const x = moving.x + e.clientX - moving.cX
        const y = moving.y + e.clientY - moving.cY

        moving.g.setAttribute('transform', `translate(${x}, ${y})`)

        moving.linesFrom?.forEach(line => {
            line.setAttribute('x1', x)
            line.setAttribute('y1', y)
        })

        moving.linesTo?.forEach(line => {
            line.setAttribute('x2', x)
            line.setAttribute('y2', y)
        })
    } else if (movingPlane) {
        const x = movingPlane.x + e.clientX - movingPlane.cX
        const y = movingPlane.y + e.clientY - movingPlane.cY

        movingPlane.plane.setAttribute('transform', `translate(${x}, ${y})`)
    }
}

function onMouseUp(e) {
    const { moving, movingPlane } = ctx

    const xxx = moving || movingPlane
    const xyKey = moving ? 'xy:' + xxx.g.id : ctx.type + ':plane:xy'

    if (!xxx) { return }

    const x = xxx.x + e.clientX - xxx.cX
    const y = xxx.y + e.clientY - xxx.cY

    localStorage.setItem(xyKey, JSON.stringify({ x, y }))

    ctx.moving = null
    ctx.movingPlane = null
}

function onPlaneDragStart(e) {
    if (e.target.id != 'canvas') { return }

    e.stopPropagation()

    const xyKey = ctx.type + ':plane:xy'

    let { x = 0, y = 0 } = JSON.parse(localStorage.getItem(xyKey) || '{}')

    ctx.movingPlane = {
        plane: $('#plane'),
        x, y,
        cX: e.clientX,
        cY: e.clientY,
    }
}

function clickEntity(e) {
    e.stopPropagation()
    location.hash = e.target.parentNode.id
}

function showEntity(nodeId) {
    if (!nodeId) {
        return closePanel()
    }

    const ents = ctx.ents
    const [type, entId] = nodeId.split(':')
    const ent = ents[type][entId]
    const title = `${type}: ${entId}`
    const errors = ctx.errors[nodeId] || {}

    if (!ent) {
        openError(`${type} not found by id: ${entId}`)
        return closePanel()
    }

    let body = ''

    const div = key => `<div class="row ${errors[key] ? 'error' : ''}">`

    body += `${div('id')}
        <div class="key">id</div>
        <div class="value" title="${entId}">${entId}</div>
    </div>`

    if (type === 'object') {
        body += `${div('class')}
            <div class="key">class</div>
            <a class="rel value" href="#class:${ent.class}">${ent.class}</a>
            <button data-key="class" class="nobg edit">edit</button>
        </div>`
    }

    Object.keys(ent).sort().forEach(key => {
        if (key === 'id') { return }
        if (key === 'class' && type === 'object') { return }

        const value = ent[key]

        if (type === 'class') {
            const [prefix, name] = key.split(':')

            if (prefix === 'rel' && name) {
                body += `${div(key)}
                    <div class="key" title="${key}">${name}</div>
                    <a class="rel value" href="#class:${value}">${value}</a>
                    <button data-key="${key}" class="nobg edit">edit</button>
                </div>`

                return
            }
        } else {
            const entClass = ents.class[ent.class]
            const relObject = entClass?.['rel:' + key]

            if (relObject) {
                body += `${div(key)}
                    <div class="key" title="${key}">${key}</div>
                    <a class="rel value" href="#object:${value}">${value}</a>
                    <button data-key="${key}" class="nobg edit">edit</button>
                </div>`

                return
            }
        }

        body += `${div(key)}
            <div class="key" title="${key}">${key}</div>
            <div class="value" title="${value}">${value}</div>
            <button data-key="${key}" class="nobg edit">edit</button>
        </div>`
    })

    if (Object.keys(errors).length > 0) {
        body += `<div class="errors">
            ${Object.entries(errors).map(([key, error]) => `
                key <b>${key}</b>: ${error}
            `).join('<br>')}
        </div>`
    }

    body += `
        <div class="space"></div>
        <div class="actions">
            <button class="del nobg">delete</button>
            <button class="add">+ key</button>
        </div>
    `

    openPanel(title, body)

    $('#panel .del').addEventListener('click', () => confirmDelEnt(nodeId))
    $('#panel .add').addEventListener('click', () => openAddKey(nodeId))
    $$('#panel .edit').forEach(btn => {
        btn.addEventListener('click',
            () => openEditKey(nodeId, btn.dataset.key))
    })
}

function confirmDelEnt(nodeId) {
    const [type, id] = nodeId.split(':')

    const title = `delete ${type}: ${id}`
    const msg = `${type} "${id}" will be permanently deleted<br>rels will be broken`

    openConfirm(title, msg, () => delEnt(nodeId))
}

function openAddKey(nodeId) {
    const [type, entId] = nodeId.split(':')
    const title = `add key to ${type}: ${entId}`

    let body = `
        <label class="row">
            <div class="key">key</div>
            <input class="value" spellcheck="false" />
        </label>
        <label class="row">
            <div class="key">value</div>
            <input class="value" spellcheck="false" />
        </label>
        <div class="actions">
            <div id="submit-error"></div>
            <button data-label="add" id="popup-submit">add</button>
        </div>
    `

    openPopup(title, body)

    $('#popup input').focus()
    $('#popup-submit').addEventListener('click', () => {
        const inputs = $$('#popup input')
        const key = inputs[0].value
        const value = inputs[1].value
        setEntKey(nodeId, key, value, 'adding')
    })
}

function openEditKey(nodeId, key) {
    if (key === 'id') { return }

    const [type, entId] = nodeId.split(':')
    const value = ctx.ents[type][entId][key]
    const title = `edit ${type}: ${entId}`

    let body = `
        <label class="row">
            <div class="key">${key}</div>
            <input class="value" value="${value}" spellcheck="false" />
        </label>
        <div class="actions">
            <div id="submit-error"></div>
            <button class="del nobg">delete</button>
            <button data-label="save" id="popup-submit">save</button>
        </div>
    `

    openPopup(title, body)

    const input = $('#popup input')
    input.selectionStart = input.value.length
    input.selectionEnd = input.value.length
    input.focus()

    $('#popup .del')?.addEventListener('click',
        () => confirmDelEntKey(nodeId, key))

    $('#popup-submit').addEventListener('click', () => {
        const value = $('#popup input').value
        setEntKey(nodeId, key, value, 'saving')
    })
}

function confirmDelEntKey(nodeId, key) {
    const [type, id] = nodeId.split(':')
    const title = `delete key: ${key}`
    const msg = `key "${key}" will be permanently deleted from the ${type} "${id}"`

    openConfirm(title, msg,
        () => delEntKey(nodeId, key),
        () => openEditKey(nodeId, key))
}

function openAdd() {
    const type = ctx.type
    const title = `add ${type}`
    const isObject = type === 'object'

    let body = `
        <div class="row kv">
            <input class="key" spellcheck="false" value="id" disabled />
            <input class="value" spellcheck="false" />
        </div>
    `

    if (isObject) {
        body += `<div class="row kv">
            <input class="key" spellcheck="false" value="class" disabled />
            <input class="value" spellcheck="false" />
        </div>`
    }

    body += `<div class="actions">
        <div id="submit-error"></div>
        <button class="add nobg">+ key</button>
        <button data-label="add" id="popup-submit">add</button>
    </div>`

    openPopup(title, body)

    $$('#popup input')[1].focus()

    $('#popup .add').addEventListener('click', () => {
        $('#submit-error').innerText = ''

        const row = d.createElement('div')

        row.classList.add('row', 'kv')
        row.innerHTML = `
            <input class="key" spellcheck="false" placeholder="key" />
            <input class="value" spellcheck="false" placeholder="value" />
            <button class="x">x</button>
        `

        $('#popup .popup-body').insertBefore(row, $('#popup .actions'))

        row.querySelector('input').focus()
        row.querySelector('.x').addEventListener('click', () => {
            row.parentNode.removeChild(row)
        })
    })

    $('#popup-submit').addEventListener('click', () => {
        const entity = {}

        const keys = $$('#popup .kv .key')
        const values = $$('#popup .kv .value')

        keys.forEach((node, i) => {
            entity[node.value || node.innerText] = values[i].value
        })

        addEnt(type, entity)
    })
}

function openConfirm(title, msg, onYes, onNo) {
    openPopup(title, `
        <div class="popup-msg">${msg}</div>
        <div class="actions">
            <div id="submit-error"></div>
            <button id="popup-cancel" class="nobg">cancel</button>
            <button id="popup-submit" data-label="confirm">confirm</button>
        </div>
    `)

    $('#popup-cancel').addEventListener('click', onNo || closePopup)
    $('#popup-submit').addEventListener('click', onYes)
}

function openError(error) {
    const msg = error instanceof Error
        ? String(error.stack)
        : String(error)

    openPopup('error', `
        <div class="popup-msg">${msg}</div>
        <div class="actions">
            <button>close</button>
        </div>
    `)

    $('#popup .actions button').addEventListener('click', closePopup)
}

function openPopup(title, body) {
    $('#popup .popup-title').innerText = title
    $('#popup .popup-body').innerHTML = body
    $('#popup-shadow').style.display = 'block'
    $('#popup').style.display = 'block'
}

function openPanel(title, body) {
    $('#panel .popup-title').innerText = title
    $('#panel .popup-body').innerHTML = body
    $('#panel').style.display = 'block'
}

function closePopup() {
    if (ctx.submitting) {
        return
    }

    $('#popup').style.display = 'none'
    $('#popup-shadow').style.display = 'none'
}

function closePanel() {
    $('#panel').style.display = 'none'
}

function onSubmitStart(actionName) {
    $$('#popup button').forEach(btn => btn.disabled = true)

    const btn = $('#popup-submit')
    const err = $('#submit-error')

    ctx.submitting = true
    btn.innerText = actionName
    err.innerText = ''
}

function onSubmitSuccess() {
    $$('#popup button').forEach(btn => btn.disabled = false)

    ctx.submitting = false

    closePopup()
}

function onSubmitError(error) {
    $$('#popup button').forEach(btn => btn.disabled = false)

    const btn = $('#popup-submit')
    const err = $('#submit-error')

    ctx.submitting = false
    btn.innerText = btn.dataset.label
    err.innerText = error
}

function checkClass(id) {
    const errors = {}

    if (!id || typeof id !== 'string' || !id.match(/^[A-Za-z0-9]+$/)) {
        errors.id = `invalid format; expected: ^[A-Za-z0-9]+$`
    }

    const ent = ctx.ents.class[id]

    Object.keys(ent).forEach(key => {
        if (key === 'id') {
            return
        }

        if (!key.includes(':')) {
            const keyType = ent[key]

            if (!ctx.classKeyTypes[keyType]) {
                const expected = Object.keys(ctx.classKeyTypes).join(', ')
                errors[key] = `invalid type (expected: ${expected})`
            }

            return
        }

        const [prefix, suffix] = key.split(':')

        if (!suffix) {
            errors[key] = 'empty suffix'
            return
        }

        if (prefix === 'rel') {
            const relClassId = ent[key]
            const relClass = ctx.ents.class[relClassId]

            if (!relClass) {
                errors[key] = `class rel not found: <b>${relClassId}</b>`
            }

            return
        }

        errors[key] = `invalid prefix (expected: rel)`
    })

    return errors
}

function checkObject(id) {
    const errors = {}

    if (!id || typeof id !== 'string' || !id.match(/^[a-z0-9_]+$/)) {
        errors.id = `invalid format; expected: ^[a-z0-9_]+$`
    }

    const ent = ctx.ents.object[id]
    const objClass = ctx.ents.class[ent.class]

    if (!objClass) {
        errors.class = `class not found: <b>${ent.class}</b>`
    }

    Object.keys(ent).forEach(key => {
        if (key === 'id' || key === 'class') { return }

        if (!objClass) {
            errors[key] = `unable to check key`
            return
        }

        const keyClassId = objClass[`rel:${key}`]
        const type = objClass[key]
        const value = ent[key]

        if (keyClassId) {
            const relId = value
            const rel = ctx.ents.object[relId]

            if (!rel) {
                errors[key] = `object rel not found: <b>${relId}</b>`
            } else if (rel.class !== keyClassId) {
                errors[key] = `rel class mismatch: <b>${rel.class}</b> (expected: ${keyClassId})`
            }

            return
        }

        if (!type) {
            errors[key] = `not defined in class`
            return
        }

        if (!value) {
            errors[key] = `empty value`
            return
        }

        if (type === 'int') {
            if (String(parseInt(value, 10)) !== value) {
                errors[key] = 'invalid int'
            }
        } else if (type === 'float') {
            if (!Number.isFinite(parseFloat(value))) {
                errors[key] = 'invalid float'
            }
        } else if (type === 'string') {
            // ok
        } else if (type === 'object') {
            if (typeof JSON.parse(value) !== 'object') {
                errors[key] = 'invalid object'
            }
        } else if (type === 'array') {
            if (!Array.isArray(JSON.parse(value))) {
                errors[key] = 'invalid array'
            }
        } else if (type === 'bool') {
            if (value !== 'true' && value !== 'false') {
                errors[key] = 'invalid bool'
            }
        } else {
            errors[key] = 'invalid type'
        }
    })

    return errors
}
