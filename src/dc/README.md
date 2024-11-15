# Füriend Directive Center - dc

## todo

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
