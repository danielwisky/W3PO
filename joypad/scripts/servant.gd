var id
var position
var limit
var step

func _init(id, position, limit, step = 5):
	self.id = id
	self.position = position
	self.limit = limit
	self.step = step

func increment():
	self.position += step

func decrement():
	self.position -= step
