package main

import "time"

type Queue struct {
	data       map[string]string
	timestamps map[string]time.Time
}

type QueueResult struct {
	slot, value string
	timestamp   time.Time
}

func NewQueue() *Queue {
	return &Queue{
		data:       make(map[string]string),
		timestamps: make(map[string]time.Time),
	}
}

func (q *Queue) Push(slot, value string) {
	q.data[slot] = value
	if _, contains := q.timestamps[slot]; !contains {
		q.timestamps[slot] = time.Unix(0, 0)
	}
}

func (q *Queue) Pop() *QueueResult {
	var oldest *QueueResult
	for k, v := range q.data {
		if oldest == nil || oldest.timestamp.After(q.timestamps[k]) {
			oldest = &QueueResult{
				slot:      k,
				value:     v,
				timestamp: q.timestamps[k],
			}
		}
	}
	if oldest != nil {
		delete(q.data, oldest.slot)
		q.timestamps[oldest.slot] = time.Now()
	}
	return oldest
}
