extends Node2D

var direction = Vector2()

func _ready():
	pass # Replace with function body.

# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta):
	pass

func _on_top_mouse_entered():
	direction.y = -1

func _on_top_mouse_exited():
	direction.y = 0
	
func _on_down_mouse_entered():
	direction.y = 1

func _on_down_mouse_exited():
	direction.y = 0

func _on_left_mouse_entered():
	direction.x = -1

func _on_left_mouse_exited():
	direction.x = 0

func _on_right_mouse_entered():
	direction.x = 1

func _on_right_mouse_exited():
	direction.x = 0
