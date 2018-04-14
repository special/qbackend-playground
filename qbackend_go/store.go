package qbackend

type Store struct {
	ld                interface{}
	identifier        string
	subscriptionCount int
	SetHook           func(*Store, string, interface{}) `json:"-"`
	RemoveHook        func(*Store, string)              `json:"-"`
}

func (this *Store) Publish(identifier string) {
	if this.identifier != "" {
		panic("Can't publish twice")
	}
	if identifier == "" {
		panic("Can't publish an empty identifier")
	}
	this.identifier = identifier
}

func (this *Store) Subscribe() {
	if this.identifier == "" {
		panic("Can't subscribe an unpublished model")
	}
	if this.subscriptionCount == 0 {
		Create(this.identifier, this.ld)
	}
	this.subscriptionCount += 1
}

func (this *Store) Update(val interface{}) {
	// ### diff ld and val and send changes, rather than the whole value.
	this.ld = val
	if this.subscriptionCount > 0 {
		Create(this.identifier, val)
	}
}
