var id
var position
var limit

func _init(id, position, limit):
	self.id = id
	self.position = position
	self.limit = limit

func increment():
	self.position += 1

func decrement():
	self.position -= 1
