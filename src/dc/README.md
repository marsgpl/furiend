# Füriend Directive Center - dc

## todo

- error strategy for logic - when to fail and how to report. rn it just exits
- on object save: save parent objects (key "-")
- save tmp objs in "t:*" (add display for ui + load them in world)
- logic event queue (when emitting events from logic itself)
- compare classes. if lots of fields are similar, suggest creating parent class
- transform event to class: single match, multiple matches, none matched
- single match: event have more fields than a class: alter class fields?
- multiple matches: ?
- none matched: highest match score - alter class fields?
- none matched: no high scores: create class?
- event fields checksum
- cache checksums to skip duplicated class altering attempts
- track those events as well
- system events should have unique field that disables altering?

on transform:
    unwrapping error
    validate result obj error
    if objects are missing - try to auto create using unwraps
    find missing fields in result obj: event
    unused_path unmapped fields: event
unable to transform: event
