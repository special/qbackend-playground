# QBackend

This is something to try to more fully decouple the data in an application from
the UI. This might be useful to avoid stability concerns, to better ease
multiple UI implementations, etc.

# Fundamentals

This is accomplished by introducing a form of communication. The communication
is envisaged as being bidirectional, but for the sake of simplicity, we will
typically talk about a single direction (backend objects -> UI) for now. Thus,
the UI is the client, and the backend is the server.

Objects have a well known identifier, and an implementation behind them that
consists of data and methods that may be invoked by the other side of the
connection (using "proxy objects"). Changes in state are communicated by
"emitting", which the remote proxy object will use to mutate itself (for
example, updating the UI in response to data changes).

Thus, the flow is generally like:

* Client starts server
* Client subscribes to some objects
* Server sends object state in response to subscription
* Client invokes methods on objects, in response, Server emits changes for client to consume

Note that the methods on objects are _deliberately_ unspecified, but this may
change, as it might make sense to give JsonListModel a standard "remove these
rows" command etc that can be implemented if removal is desired.

# JsonListModel

A JsonListModel provides an (unordered) set of JSON objects. Each JSON object is
given a unique identifier to provide to make later updates (or removals) easy.

## Introduction

    { "data": { "row1": { ... }, "row2": { ... } } }

## Emissions

### Set

Notifies about the insertion (or change) of a row.

    { "UUID": "row1", "data", { ... }}

### Remove

Notifies about the removal of a row.

    { "UUID": "row1" }

# Store

A Store provides access to the properties of a JSON object.

## Introduction

    { "prop1": "val", ... }


## Emissions

Note that for the time being, changes to Store are also given through
introduction messages, rather than emissions. This should perhaps change.

# Future work

* We need a way to sort and filter JsonListModels. A QML-friendly
  SortFilterProxyModel, of sorts, is what I have in mind.
* A more scalable model type (JsonListModel will be O(n) for pretty much any
  changes) is desirable, but needs some thought: QAbstractListModel is a complex
  API, and mapping it directly may not be desirable.
* Store might need defined emissions rather than using introduction messages for
  any changes.
* JsonListModel might benefit from a standardized set/remove method API that can
  be implemented if desired (thus leaving invokeMethod for only "custom" methods,
  giving the benefit of stronger typing and easier use on the UI side).
