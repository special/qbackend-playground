package qbackend

import (
	"fmt"
	"sort"
)

// SortableModel can be implemented by models to use the SortModel functions,
// which handle logic to keep models sorted during insertions and changes.
type SortableModel interface {
	// Inherently implemented by models
	ModelDataSource
	Inserted(start, count int)

	// RowLess is a less function, equivalent to the sort package
	RowLess(i, j int) bool
	// RowMove should move row 'src' to index 'dst', without emitting signals
	RowMove(src, dst int)
}

// Sort newly inserted rows [start:end] into positions <= start and emit signals
func SortModelInserted(model SortableModel, start, end int) {
	mvStart, mvEnd := -1, -1
	emitCount := 0

	for i := start; i < end; i++ {
		n := sort.Search(i, func(j int) bool { return model.RowLess(i, j) })
		if i != n {
			model.RowMove(i, n)
		}

		if mvStart < 0 && mvEnd < 0 {
			mvStart = n
			mvEnd = n
		} else if n >= mvStart && n <= mvEnd+1 {
			mvEnd = mvEnd + 1
		} else {
			model.Inserted(mvStart, mvEnd-mvStart+1)
			emitCount += (mvEnd - mvStart) + 1
			mvStart = n
			mvEnd = n
		}
	}

	if mvStart >= 0 && mvEnd >= 0 {
		model.Inserted(mvStart, mvEnd-mvStart+1)
		emitCount += (mvEnd - mvStart) + 1
	}

	if emitCount != end-start {
		panic(fmt.Sprintf("emitted moves for %d rows, insert had %d", emitCount, end-start))
	}
}
