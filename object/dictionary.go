package object

import (
	"bytes"
	"encoding/binary"
	"errors"
	"fmt"
	"hash/fnv"
	"math"
	"reflect"
)

const (
	DictionaryInitialCap = 32
	MaxLoadFactor        = 0.75
	MaxMovePerPut        = 8
	MaxMovePerGet        = 8
	MaxMovePerDelete     = 8
)

var (
	ErrUnhashableType = errors.New("unhashable type")
)

func hashObject(obj Object) (uint64, error) {
	r := obj.Raw()
	h := fnv.New64()
	var b []byte
	switch v := r.(type) {
	case string:
		b = []byte(v)
	case int64:
		b = make([]byte, 8)
		binary.LittleEndian.PutUint64(b, uint64(v))
	case bool:
		if v {
			b = []byte{1}
		} else {
			b = []byte{0}
		}
	case float64:
		b = make([]byte, 8)
		binary.LittleEndian.PutUint64(b, math.Float64bits(v))
	default:
		return 0, ErrUnhashableType
	}
	_, err := h.Write(b)
	return h.Sum64(), err
}

func equalObjects(a, b Object) bool {
	return a.Type() == b.Type() && reflect.DeepEqual(a.Raw(), b.Raw())
}

type KeyValue struct {
	Key   Object
	Value Object
	next  *KeyValue
}

type Dictionary struct {
	defaultObject
	items    []*KeyValue
	oldItems []*KeyValue
	len      int
	cap      int
	oldCap   int
	movePtr  int
}

var _ Object = (*Dictionary)(nil)

func NewDictionary2() *Dictionary {
	return &Dictionary{
		items:   make([]*KeyValue, DictionaryInitialCap),
		len:     0,
		cap:     DictionaryInitialCap,
		movePtr: -1,
	}
}

func (d *Dictionary) Put(key Object, value Object) error {
	if d.movePtr >= 0 {
		if err := d.moveItems(MaxMovePerPut); err != nil {
			return err
		}
	}
	h, err := hashObject(key)
	if err != nil {
		return err
	}
	h %= uint64(d.cap)
	var kv *KeyValue
	ptr := d.items[h]
	for ptr != nil {
		if equalObjects(ptr.Key, key) {
			kv = ptr
			break
		}
		ptr = ptr.next
	}

	if kv == nil {
		kv = &KeyValue{
			Key:   key,
			Value: value,
		}
		kv.next = d.items[h]
		d.items[h] = kv
	} else {
		kv.Value = value
	}
	d.len++

	if float64(d.len)/float64(d.cap) > MaxLoadFactor {
		return d.grow()
	}

	return nil
}

func (d *Dictionary) Get(key Object) (Object, bool, error) {
	if d.movePtr >= 0 {
		if err := d.moveItems(MaxMovePerGet); err != nil {
			return nil, false, err
		}
	}

	h, err := hashObject(key)
	if err != nil {
		return nil, false, err
	}
	h1 := h % uint64(d.cap)
	ptr := d.items[h1]
	for ptr != nil {
		if equalObjects(ptr.Key, key) {
			return ptr.Value, true, nil
		}
		ptr = ptr.next
	}
	if d.oldItems != nil {
		h2 := h % uint64(d.oldCap)
		ptr = d.oldItems[h2]
		for ptr != nil {
			if equalObjects(ptr.Key, key) {
				return ptr.Value, true, nil
			}
			ptr = ptr.next
		}
	}
	return nil, false, nil
}

func (d *Dictionary) Delete(key Object) (bool, error) {
	if d.movePtr >= 0 {
		if err := d.moveItems(MaxMovePerDelete); err != nil {
			return false, err
		}
	}

	h, err := hashObject(key)
	if err != nil {
		return false, err
	}

	h1 := h % uint64(d.cap)
	var prev, next, ptr *KeyValue
	ptr = d.items[h1]
	deleted := false
	for ptr != nil {
		if equalObjects(ptr.Key, key) {
			next = ptr.next
			if prev == nil {
				d.items[h1] = next
			} else {
				prev.next = next
			}
			d.len--
			deleted = true
			break
		}
		prev = ptr
		ptr = ptr.next
	}

	if d.oldItems != nil {
		h2 := h % uint64(d.oldCap)
		prev = nil
		ptr = d.oldItems[h2]
		for ptr != nil {
			if equalObjects(ptr.Key, key) {
				next = ptr.next
				if prev == nil {
					d.oldItems[h2] = next
				} else {
					prev.next = next
				}
				d.len--
				deleted = true
				break
			}
			prev = ptr
			ptr = ptr.next
		}
	}

	return deleted, nil
}

func (d *Dictionary) grow() error {
	if d.oldItems != nil {
		return fmt.Errorf("cannot grow dictionary while resizing is in progress")
	}
	newItems := make([]*KeyValue, d.cap*2)
	d.oldItems = d.items
	d.oldCap = d.cap
	d.items = newItems
	d.cap *= 2
	d.movePtr = 0
	return nil
}

func (d *Dictionary) moveItems(maxMoves int) error {
	if d.oldItems == nil {
		return nil
	}
	for range maxMoves {
		if d.movePtr >= d.oldCap {
			d.oldItems = nil
			d.oldCap = 0
			d.movePtr = -1
			return nil
		}
		ptr := d.oldItems[d.movePtr]
		for ptr != nil {
			h, err := hashObject(ptr.Key)
			if err != nil {
				return err
			}
			h %= uint64(d.cap)
			ptr2 := d.items[h]
			var newkv *KeyValue
			for ptr2 != nil {
				if equalObjects(ptr2.Key, ptr.Key) {
					goto NextKey
				}
				ptr2 = ptr2.next
			}
			newkv = &KeyValue{
				Key:   ptr.Key,
				Value: ptr.Value,
			}
			newkv.next = d.items[h]
			d.items[h] = newkv
		NextKey:
			ptr = ptr.next
		}
		d.oldItems[d.movePtr] = nil
		d.movePtr++
	}
	return nil
}

func NewDictionary() *Dictionary {
	cap := DictionaryInitialCap
	return &Dictionary{
		items: make([]*KeyValue, cap),
		cap:   cap,
		len:   0,
	}
}

func (d *Dictionary) Clone() Object {
	newDict := NewDictionary()
	for _, kv := range d.items {
		ptr := kv
		for ptr != nil {
			newDict.Put(ptr.Key, ptr.Value)
			ptr = ptr.next
		}
	}
	return newDict
}

func NewDictionaryWithItems(items []Object) (*Dictionary, error) {
	d := NewDictionary()
	for _, item := range items {
		if !item.IsList() || item.(*List).Len() != 2 {
			return nil, errors.New("dictionary items must be lists of key-value pairs")
		}
		kv := item.(*List)
		if err := d.Put(kv.items[0], kv.items[1]); err != nil {
			return nil, err
		}
	}
	return d, nil
}

func (d *Dictionary) Raw() any {
	m := make(map[any]any)
	for _, kv := range d.items {
		ptr := kv
		for ptr != nil {
			m[ptr.Key.Raw()] = ptr.Value.Raw()
			ptr = ptr.next
		}
	}
	return m
}

func (d *Dictionary) Type() ObjectType {
	return DictionaryType
}

// TODO: if I ever come up with a first-class syntax for dictionaries, update this method
func (d *Dictionary) String() string {
	var b bytes.Buffer
	b.WriteString("(dict")

	for _, kv := range d.items {
		ptr := kv
		for ptr != nil {
			b.WriteString(" [")
			b.WriteString(ptr.Key.String())
			b.WriteByte(' ')
			b.WriteString(ptr.Value.String())
			b.WriteByte(']')
			ptr = ptr.next
		}
	}

	b.WriteString(")")
	return b.String()
}

func (d *Dictionary) IsDictionary() bool {
	return true
}

func (d *Dictionary) Len() int {
	return d.len
}

func (d *Dictionary) IsLenable() bool {
	return true
}

func (d *Dictionary) Keys() []Object {
	keys := make([]Object, 0, d.len)
	if d.oldItems != nil {
		for _, kv := range d.oldItems {
			ptr := kv
			for ptr != nil {
				keys = append(keys, ptr.Key)
				ptr = ptr.next
			}
		}
	}
	for _, kv := range d.items {
		ptr := kv
		for ptr != nil {
			keys = append(keys, ptr.Key)
			ptr = ptr.next
		}
	}
	return keys
}

func (d *Dictionary) Values() []Object {
	values := make([]Object, 0, d.len)
	if d.oldItems != nil {
		for _, kv := range d.oldItems {
			ptr := kv
			for ptr != nil {
				values = append(values, ptr.Value)
				ptr = ptr.next
			}
		}
	}
	for _, kv := range d.items {
		ptr := kv
		for ptr != nil {
			values = append(values, ptr.Value)
			ptr = ptr.next
		}
	}
	return values
}
