const w = window
const d = document
const $ = d.querySelector.bind(d)
const $$ = d.querySelectorAll.bind(d)

const ctx = {
    mode: localStorage.getItem('mode') || 'class',
}

d.addEventListener('DOMContentLoaded', () => {
    w.addEventListener('error', openPopupError)
    w.addEventListener('mousemove', onMouseMove)
    w.addEventListener('mouseup', onMouseUp)
    w.addEventListener('hashchange', togglePanel)

    fetch('/entities').then(r => r.json()).then(data => {
        assert(!data?.error, data.error)

        ctx.errors = new Map([
            ['class', new Map],
            ['object', new Map],
        ])

        ctx.warnings = new Map([
            ['class', new Map],
            ['object', new Map],
        ])

        ctx.types = new Set(data.types)
        ctx.ents = {
            class: data.classes,
            object: data.objects,
        }

        checkEnts('class')
        checkEnts('object')
        draw()
        toggleStats()
        togglePanel()
    }).catch(openPopupError)

    $('#mode').value = ctx.mode
    $('#mode').addEventListener('change', () => {
        ctx.mode = $('#mode').value
        localStorage.setItem('mode', ctx.mode)
        draw()
        toggleStats()
    })

    $('#canvas').addEventListener('mousedown', startDraggingPlane)
    $('#popup-shadow').addEventListener('click', closePopup)
    $('#popup .popup-close').addEventListener('click', closePopup)
    $('#panel .popup-close').addEventListener('click', () => location.hash = '')
    $('#add-ent').addEventListener('click', openAddEnt)
})

function perf(label) {
    if (!this.ts) {
        this.ts = Date.now()
    } else {
        console.log(label + ':', Date.now() - this.ts, 'ms')
        this.ts = undefined
    }
}

function toggleStats() {
    const stats = $('#stats')

    const errsN = ctx.errors.get(ctx.mode).size
    const warnsN = ctx.warnings.get(ctx.mode).size

    if (errsN || warnsN) {
        const text = []

        errsN && text.push(`red: ${errsN}`)
        warnsN && text.push(`yellow: ${warnsN}`)

        stats.innerText = text.join('\n')
        stats.style.display = 'block'
    } else {
        stats.innerText = ''
        stats.style.display = 'none'
    }
}

function assert(cond, msg) {
    if (!cond) {
        throw Error(msg || 'assertion failed')
    }
    return cond
}

function esc(string) {
    return String(string).replace(/[>"<]/g, s => esc.rep[s])
}
esc.rep = {
    '<': '&lt;',
    '>': '&gt;',
    '"': '&quot;',
}

function checkId(id) {
    assert(id.match(checkId.expr), 'invalid id')
}
checkId.expr = /^[a-z0-9_-]+$/

function checkKeyName(key) {
    assert(key.match(checkKeyName.expr), 'invalid key name')
}
checkKeyName.expr = /^[a-z0-9_-]+$/

function checkClassKey(ent, key) {
    let schema = ent[key]

    if (typeof schema !== 'object') {
        schema = JSON.parse(ent[key])
        assert(typeof schema === 'object', 'schema is not an object')
        ent[key] = schema
    }

    const type = schema.type
    assert(ctx.types.has(type), `schema type not found: ${type}`)

    if (type === 'rel' || type === 'rels') {
        const relClass = ctx.ents.class[schema.class]
        assert(relClass, `rel class not found: ${schema.class}`)
    }
}

function checkObjectKey(ent, key, entClass) {
    assert(entClass, `object class not found: ${ent.class}`)
    if (key === 'class') return

    const schema = entClass[key]
    assert(schema, 'key not found in class definition')

    const linker = checkObjectKey.linkers[schema.type]
    assert(linker, `linker not found for the key type: ${schema.type}`)

    linker(ent[key], schema)
}
checkObjectKey.linkers = {
    bool: value => assert(value === 'true' || value === 'false', 'bad bool'),
    int: value => assert(String(parseInt(value, 10)) === value, 'bad int'),
    float: value => assert(Number.isFinite(parseFloat(value)), 'bad float'),
    str: () => {},
    table: value => {
        value = JSON.parse(value)
        assert(typeof value === 'object', 'not an Array or Object')
    },
    object: value => {
        value = JSON.parse(value)
        assert(!Array.isArray(value), 'object expected, have: array')
        assert(typeof value === 'object', 'not an object')
    },
    rel: (relId, schema) => {
        const rel = ctx.ents.object[relId]
        assert(rel, `rel object not found: ${relId}`)

        const needClassId = schema.class
        const haveClassId = rel.class
        assert(needClassId == haveClassId,
            `rel class mismatch: ${needClassId} != ${haveClassId}`)
    },
    rels: (rels, schema) => {
        rels = JSON.parse(rels)
        assert(Array.isArray(rels), 'not an array')
        rels.forEach(rel => checkObjectKey.linkers.rel(rel, schema))
    },
    class: (relId) => {
        const rel = ctx.ents.class[relId]
        assert(rel, `rel class not found: ${relId}`)
    },
}

function checkEnt(mode, ent, id) {
    ent.id = id

    const errors = new Map
    const warnings = new Map

    const isObject = mode === 'object'
    const entClass = isObject && ctx.ents.class[ent.class]

    for (key in ent) {
        try {
            if (key === 'id') {
                checkId(id)
            } else {
                checkKeyName(key)

                mode === 'class'
                    ? checkClassKey(ent, key)
                    : checkObjectKey(ent, key, entClass)
            }
        } catch (error) {
            errors.set(key, error.message)
        }
    }

    if (isObject) {
        if (!ent.class) {
            errors.set('class', 'class field not found')
        }

        if (entClass) {
            Object.entries(entClass).forEach(([key, schema]) => {
                if (ent[key] === undefined) {
                    const type = schema?.type
                    const isRel = type === 'rel'
                    const isMultiRel = type === 'rels'

                    if (isRel || isMultiRel) {
                        warnings.set(key, `is missing: class ${schema.class}${isMultiRel ? '[]' : ''}`)
                    } else {
                        warnings.set(key, `is missing: ${type}`)
                    }
                }
            })
        }
    }

    if (errors.size) {
        ctx.errors.get(mode).set(id, errors)
    } else {
        ctx.errors.get(mode).delete(id)
    }

    if (warnings.size) {
        ctx.warnings.get(mode).set(id, warnings)
    } else {
        ctx.warnings.get(mode).delete(id)
    }
}

function checkEnts(mode) {
    perf()

    const ents = ctx.ents[mode]

    for (const id in ents) {
        checkEnt(mode, ents[id], id)
    }

    perf(`${mode} check`)
}

function onSubmitStart(statusText) {
    ctx.isPopupBusy = true

    const btn = Array.from($$('#popup button')).pop()
    btn.disabled = true
    btn.dataset.text = btn.innerText
    btn.innerText = statusText

    $('#submit-errmsg')?.remove()
}

function onSubmitError(error) {
    ctx.isPopupBusy = false

    const btn = Array.from($$('#popup button')).pop()
    btn.disabled = false
    btn.innerText = btn.dataset.text

    const errmsg = d.createElement('div')
    errmsg.id = 'submit-errmsg'
    errmsg.innerText = error?.message || error

    $('#popup .popup-actions').prepend(errmsg)
}

function onSubmitSuccess() {
    ctx.isPopupBusy = false
    closePopup()
}

function setEntKey(fullId, key, newKey, value) {
    const [mode, id] = fullId.split(/:(.*)/)
    const ent = ctx.ents[mode][id]

    onSubmitStart('saving')

    fetch(`/${mode}/key`, {
        method: 'PUT',
        body: JSON.stringify({
            id,
            key,
            new_key: newKey,
            value,
        }),
    }).then(r => r.json()).then(data => {
        assert(!data?.error, data.error)
        onSubmitSuccess()

        if (key === 'id') {
            delete ctx.ents[mode][id]
            ctx.errors.get(mode).delete(id)
            ctx.warnings.get(mode).delete(id)

            ctx.ents[mode][value] = ent
            const { x, y } = loadXY(`xy:${fullId}`)
            saveXY(`xy:${mode}:${value}`, x, y)

            mode === 'class' && checkEnts('class')
            checkEnts('object')
            draw()
            toggleStats()
            location.hash = `${mode}:${value}`
        } else {
            delete ent[key]
            ent[newKey] = value

            mode === 'class' && checkEnts('class')
            checkEnts('object')
            draw()
            toggleStats()
            togglePanel()
        }
    }).catch(onSubmitError)
}

function addEnt(mode, entity) {
    onSubmitStart('saving')

    fetch(`/${mode}`, {
        method: 'PUT',
        body: JSON.stringify(entity),
    }).then(r => r.json()).then(data => {
        assert(!data?.error, data.error)
        onSubmitSuccess()

        ctx.ents[mode][entity.id] = entity

        mode === 'class' && checkEnts('class')
        checkEnts('object')
        draw()
        toggleStats()
        location.hash = `${mode}:${entity.id}`
    }).catch(onSubmitError)
}

function delEnt(fullId) {
    const [mode, id] = fullId.split(/:(.*)/)

    onSubmitStart('deleting')

    fetch(`/${mode}`, {
        method: 'DELETE',
        body: JSON.stringify({ id }),
    }).then(r => r.json()).then(data => {
        assert(!data?.error, data.error)
        onSubmitSuccess()

        delete ctx.ents[mode][id]
        ctx.errors.get(mode).delete(id)
        ctx.warnings.get(mode).delete(id)

        mode === 'class' && checkEnts('class')
        checkEnts('object')
        draw()
        toggleStats()
        location.hash = ''
    }).catch(onSubmitError)
}

function delEntKey(fullId, key) {
    const [mode, id] = fullId.split(/:(.*)/)
    const ent = ctx.ents[mode][id]

    onSubmitStart('deleting')

    fetch(`/${mode}/key`, {
        method: 'DELETE',
        body: JSON.stringify({ id, key }),
    }).then(r => r.json()).then(data => {
        assert(!data?.error, data.error)
        onSubmitSuccess()

        delete ent[key]

        mode === 'class' && checkEnts('class')
        checkEnts('object')
        draw()
        toggleStats()
        togglePanel()
    }).catch(onSubmitError)
}
