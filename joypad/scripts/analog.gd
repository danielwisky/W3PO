extends Node2D

@onready
var radius = ($base.texture.get_width() * 3) / 2

var direction = Vector2()
var actuation = 0
var pressed = false

# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta):
	if direction.distance_to($base.position) > radius:
		direction = direction.normalized() * radius

	$base/stick.global_position = direction + global_position
	actuation = $base/stick.position.distance_to($base.position)

func _input(event):
	if event is InputEventMouseButton:
		if event.is_pressed() and event.position.distance_to(position) < 250:
				pressed = true
		else: 
			direction = Vector2()
			pressed = false

	if pressed:
		direction = event.position - position

