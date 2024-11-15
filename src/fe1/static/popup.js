function actionsHTML(actions) {
    if (!actions?.length) return ''

    const btns = actions.map(({ text, className }) =>
        `<button class="${esc(className) || ''}">${esc(text)}</button>`)

    return `<div class="popup-actions">
        <div class="space"></div>
        ${btns.join('')}
    </div>`
}

function bindActions(container, actions, defaultOnClick) {
    if (!actions?.length) return

    container.querySelectorAll('.popup-actions button')?.forEach((btn, i) => {
        const onClick = actions[i]?.onClick || defaultOnClick
        onClick && btn.addEventListener('click', onClick)
    })
}

function openPopup(title, body, actions) {
    body += actionsHTML(actions)

    $('#popup .popup-title').innerText = title
    $('#popup .popup-body').innerHTML = body

    $('#popup-shadow').style.display = 'block'
    $('#popup').style.display = 'flex'

    w.addEventListener('keydown', onEscClosePopup)
    bindActions($('#popup'), actions, closePopup)
}

function closePopup() {
    if (ctx.isPopupBusy) return

    $('#popup').style.display = 'none'
    $('#popup-shadow').style.display = 'none'

    w.removeEventListener('keydown', onEscClosePopup)
}

function onEscClosePopup(event) {
    if (event.key === 'Escape') {
        closePopup()
    }
}

function openPopupError(error) {
    const msg = error instanceof Error
        ? error.stack
        : error instanceof ErrorEvent
        ? error.error.stack
        : String(error)

    openPopup('error',
        `<div class="popup-msg">${esc(msg)}</div>`,
        [{ text: 'close', className: 'clear' }])
}

function openPanel(title, body) {
    $('#panel .popup-title').innerText = title
    $('#panel .popup-body').innerHTML = body
    $('#panel').style.display = 'flex'
}

function closePanel() {
    $('#panel').style.display = 'none'
}

function togglePanel() {
    $('circle.current')?.classList.remove('current')

    let fullId = location.hash.substring(1)
    if (!fullId) return closePanel()
    if (fullId.includes('%')) {
        try { fullId = decodeURIComponent(fullId) }
        catch {}
    }

    d.getElementById(fullId)?.querySelector('circle').classList.add('current')

    const [mode, id] = fullId.split(/:(.*)/)
    const ent = ctx.ents[mode]?.[id]

    if (!ent) {
        openPopupError(`id not found: ${fullId}`)
        return closePanel()
    }

    const errors = ctx.errors.get(mode).get(id)
    const warnings = ctx.warnings.get(mode).get(id)
    const title = `${mode}: ${id}`

    const panelRowHTML = mode === 'class'
        ? panelRowClassHTML
        :panelRowObjectHTML

    const actions = [
        {
            text: 'delete',
            onClick: () => confirmDelEnt(fullId),
            className: 'clear',
        },
        {
            text: '+ key',
            onClick: () => openAddKey(fullId),
        },
    ]

    let body = panelRowHTML('id', ent)

    if (mode === 'object') {
        body += panelRowHTML('class', ent)
    }

    for (const key of Object.keys(ent).sort()) {
        if (key === 'id') continue
        if (key === 'class' && mode === 'object') continue
        body += panelRowHTML(key, ent)
    }

    body += noticeHTML(errors, 'errors')
    body += noticeHTML(warnings, 'warnings')
    body += '<div class="space"></div>'
    body += actionsHTML(actions)

    openPanel(title, body)
    bindActions($('#panel'), actions)

    $$('#panel .view-row button').forEach(btn =>
        btn.addEventListener('click',
            () => openEditKey(fullId, btn.dataset.key)))
}

function panelRowClassHTML(key, ent) {
    const error = ctx.errors.get('class').get(ent.id)?.get(key)
    const schema = ent[key]
    const type = schema?.type
    const isRel = type === 'rel'
    const isMultiRel = type === 'rels'

    let isText = true
    let value = type

    if (error) {
        value = error
    } else if (key === 'id') {
        value = ent.id
    } else if (isRel || isMultiRel) {
        const classId = schema?.class
        isText = false
        value = valueLink('class', classId, isMultiRel)
    }

    return panelRowEntityHTML(key, value, isText, error)
}

function panelRowObjectHTML(key, ent) {
    const error = ctx.errors.get('object').get(ent.id)?.get(key)
    const entClass = ctx.ents.class[ent.class]

    let isText = true
    let value = ent[key]

    if (error) {
        value = error
    } else if (key === 'id') {
        value = ent.id
    } else if (key === 'class') {
        isText = false
        value = valueLink('class', ent.class)
    } else {
        const type = entClass?.[key]?.type
        const isRel = type === 'rel'
        const isMultiRel = type === 'rels'

        if (isRel) {
            isText = false
            value = valueLink('object', ent[key])
        } else if (isMultiRel) {
            try {
                const rels = JSON.parse(value)
                isText = false
                value = `<div class="value dots multirel">${rels?.map(rel =>
                    `<a href="#object:${esc(rel)}">
                        ${esc(rel || 'undefined')}
                    </a>`).join('')}
                </div>`
            } catch (error) {
                value = error.message
            }
        }
    }

    return panelRowEntityHTML(key, value, isText, error)
}

function valueLink(mode, id, isMultiRel) {
    return `<a class="value" href="#${mode}:${esc(id)}">
        ${esc(id)}${isMultiRel ? '[]' : ''}
    </a>`
}

function panelRowEntityHTML(key, value, isTextValue, hasError) {
    return `<div class="view-row ${hasError ? 'error' : ''}">
        <div class="key dots">${esc(key)}</div>
        ${isTextValue ? `<div class="value dots">${esc(value)}</div>` : value}
        <button data-key="${esc(key)}" class="edit clear">edit</button>
    </div>`
}

function noticeHTML(kv, type) {
    if (!kv?.size) return ''

    const rows = Array.from(kv.entries())

    return `<div class="notice ${esc(type)}">
        ${rows.map(([key, value]) =>
            `<div>key <b>${esc(key)}</b>: ${esc(value)}</div>`
        ).join('')}
        <span class="notice-label">${esc(type)}: ${rows.length}</span>
    </div>`
}

function confirmDelEnt(fullId) {
    const [mode, id] = fullId.split(/:(.*)/)
    const title = `delete ${mode}: ${id}`
    const msg = `${mode} <b>${esc(id)}</b> will be permanently deleted`
        + '\nrels will be broken'

    openConfirm(title, msg, () => delEnt(fullId))
}

function openConfirm(title, msg, onYes, onNo) {
    openPopup(title, `<div class="popup-msg">${msg}</div>`, [
        { text: 'cancel', className: 'clear', onClick: onNo },
        { text: 'confirm', onClick: onYes },
    ])
}

function openEditKey(fullId, key) {
    const [mode, id] = fullId.split(/:(.*)/)
    const title = `edit key of ${mode}: ${id}`
    const isObject = mode === 'object'
    const isKeyRO = key === 'id' || (isObject && key === 'class')
    let value = ctx.ents[mode][id][key]

    if (typeof value === 'object') {
        value = JSON.stringify({ ...value, relClass: undefined })
    }

    const body = isKeyRO
        ?
        `<div class="mod-row">
            <div class="key dots">${esc(key)}</div>
            <input class="value" value="${esc(value)}" spellcheck="false" />
        </div>`
        :
        `<div class="mod-row">
            <div class="key">key</div>
            <input class="value" value="${esc(key)}" spellcheck="false" />
        </div>
        <div class="mod-row">
            <div class="key">value</div>
            <input class="value" value="${esc(value)}" spellcheck="false" />
        </div>
        ${isObject ? '' : typeHintHTML()}`

    const actions = []

    if (!isKeyRO) {
        actions.push({
            text: 'delete',
            className: 'clear',
            onClick: () => confirmDelEntKey(fullId, key),
        })
    }

    actions.push({
        text: 'save',
        onClick: () => {
            const inputs = $$('#popup input')
            const newKey = isKeyRO ? key             : inputs[0].value
            const value  = isKeyRO ? inputs[0].value : inputs[1].value
            setEntKey(fullId, key, newKey, value)
        }
    })

    openPopup(title, body, actions)
    !isObject && bindTypeHint()

    const input = $('#popup input')
    input.selectionStart = input.value.length
    input.selectionEnd = input.value.length
    input.focus()
}

function confirmDelEntKey(fullId, key) {
    const [mode, id] = fullId.split(/:(.*)/)
    const title = `delete key: ${key}`
    const msg = `key <b>${esc(key)}</b> will be permanently deleted`
        + ` from the ${mode} <b>${esc(id)}</b>`

    openConfirm(title, msg,
        () => delEntKey(fullId, key),
        () => openEditKey(fullId, key))
}

function openAddKey(fullId) {
    const [mode, id] = fullId.split(/:(.*)/)
    const title = `add key to ${mode}: ${id}`
    const isObject = mode === 'object'

    let body = `
        <div class="mod-row">
            <div class="key dots">key</div>
            <input class="value" spellcheck="false" />
        </div>
        ${isObject ? keyHintHTML(fullId, 'new-key') : ''}
        <div class="mod-row">
            <div class="key dots">value</div>
            <input class="value" spellcheck="false" />
        </div>
        ${isObject ? '' : typeHintHTML()}
    `

    openPopup(title, body, [
        { text: 'save', onClick: () => {
            const inputs = $$('#popup input')
            const key = inputs[0].value
            const value = inputs[1].value
            setEntKey(fullId, key, key, value)
        } },
    ])

    !isObject && bindTypeHint()
    isObject && bindKeyHint()

    $('#popup input').focus()
}

function onKeyHint(e) {
    const key = e.target.innerText
    const [inputKey, inputValue] = Array.from($$('#popup input'))
    const isMultiRel = key.match(/\[\]$/)

    inputKey.value = key.replace(/\[\]$/, '')

    if (isMultiRel) {
        inputValue.value = '[""]'
        inputValue.selectionStart = 2
        inputValue.selectionEnd = 2
    } else {
        inputValue.value = ''
    }

    inputValue.focus()
}

function onTypeHint(e) {
    const type = e.target.innerText
    const isRel = type === 'rel' || type === 'rels'
    const input = Array.from($$('#popup input')).pop()

    if (isRel) {
        const value = `{"type":"${type}","class":"XXX"}`
        input.value = value
        input.selectionStart = 9 + 11 + type.length
        input.selectionEnd = 9 + 11 + type.length + 3
    } else {
        const value = `{"type":"${type}"}`
        input.value = value
        input.selectionStart = 9
        input.selectionEnd = 9 + type.length
    }

    input.focus()
}

function typeHintHTML(className) {
    return `<div class="hint ${className || ''}">
        ${Array.from(ctx.types).map(type => `<u>${type}</u>`).join('')}
    </div>`
}

function keyHintHTML(fullId, className) {
    const [mode, id] = fullId.split(/:(.*)/)
    const keys = Array.from(ctx.warnings.get(mode).get(id)?.keys() || [])
    if (!keys.length) return ''
    const obj = ctx.ents.object[id]
    const objClass = ctx.ents.class[obj?.class]

    return `<div class="hint ${className || ''}">
        ${keys.map(key => {
            const isMultiRel = objClass[key]?.type === 'rels'
            return `<u>${key}${isMultiRel ? '[]' : ''}</u>`
        }).join('')}
    </div>`
}

function bindTypeHint() {
    $$('#popup .hint u')?.forEach(u => u.addEventListener('click', onTypeHint))
}

function bindKeyHint() {
    $$('#popup .hint u')?.forEach(u => u.addEventListener('click', onKeyHint))
}

function openAddEnt() {
    const { mode } = ctx
    const title = `add ${mode}`
    const isObject = mode === 'object'
    const body = isObject ? '' : typeHintHTML('new-ent')

    openPopup(title, body, [
        { text: '+ key', onClick: () => addNewRow() },
        { text: 'save', onClick: () => {
            const entity = {}

            const keys = $$('#popup .new-row .key')
            const values = $$('#popup .new-row .value')

            keys.forEach((key, i) => {
                entity[key.value] = values[i].value
            })

            addEnt(mode, entity)
        } },
    ])

    addNewRow('id')
    isObject && addNewRow('class')
    !isObject && bindTypeHint()

    $('#popup input:not([disabled])').focus()
}

function addNewRow(keyName) {
    const key = keyName
        ? `<input class="key" value="${esc(keyName)}" disabled />`
        : '<input class="key" spellcheck="false" placeholder="key" />'
    const val = `<input class="value" spellcheck="false" placeholder="value" />`
    const del = keyName ? '' : '<button class="clear">x</button>'

    const row = d.createElement('div')
    row.classList.add('new-row')
    row.innerHTML = key + val + del

    const marker = $('#popup .hint') || $('#popup .popup-actions')
    $('#popup .popup-body').insertBefore(row, marker)
    row.querySelector('input').focus()
    row.querySelector('button')?.addEventListener('click',
        () => row.parentNode.removeChild(row))
}
