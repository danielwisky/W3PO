extends Node2D

var Servant = preload('res://scripts/servant.gd')

var socket = WebSocketPeer.new()
var old_direction = Vector2()
var connected = false

# Directional
var arm_height = Servant.new(1, 90, Vector2(60, 180))
var arm_distance = Servant.new(2, 90, Vector2(100, 180))
var base = Servant.new(3, 90, Vector2(20, 170))
var claw = Servant.new(0, 180, Vector2(0, 180))

# Called when the node enters the scene tree for the first time.
func _ready():
	socket.connect_to_url("ws://192.168.4.1/websocket")	
	old_direction = $analog.direction.normalized()

# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta):
	socket.poll()
	var state = socket.get_ready_state()	
	if state == WebSocketPeer.STATE_OPEN:
		if (!connected):
			print_console("[color=\"#8BC34A\"]Connected[/color]")
			connected = true
		while socket.get_available_packet_count():
			var packet = JSON.parse_string(socket.get_packet().get_string_from_utf8())
			if (packet != null):	
				var battery = packet.battery * 100 / 8400
				update_battery(battery)
	elif state == WebSocketPeer.STATE_CLOSING:
		pass
	elif state == WebSocketPeer.STATE_CLOSED:
		var code = socket.get_close_code()
		var reason = socket.get_close_reason()
		print_console("[color=\"#F44336\"]WebSocket closed with code: " + str(code) + "[/color]")
		connected = false
		set_process(false) # Stop processing.

	move_servant($directional_one.direction.y, arm_height)
	move_servant($directional_one.direction.x, base)
	move_servant($directional_two.direction.y, arm_distance)
	move_servant($directional_two.direction.x, claw)

	var direction = $analog.direction.normalized()
	if not old_direction.is_equal_approx(direction):
		old_direction = direction
		var speed = direction * Vector2(100, -100)
		var speedLeft = clamp(roundi(speed.y + speed.x), -100, 100)
		var speedRight = clamp(roundi(speed.y - speed.x), -100, 100)
		socket.send_text(str({ "speedLeft": speedLeft, "speedRight": speedRight }))

func print_console(text):
	$console/container/log.text = $console/container/log.text + text + "\n"

func move_servant(direction, servant):
	if direction < 0 and servant.position < servant.limit.y:
		servant.increment()
		print(str({ "servant": servant.id, "position": servant.position }))
		socket.send_text(str({ "servant": servant.id, "position": servant.position }))
	elif direction > 0 and servant.position > servant.limit.x:
		servant.decrement()
		print(str({ "servant": servant.id, "position": servant.position }))
		socket.send_text(str({ "servant": servant.id, "position": servant.position }))

func update_battery(battery):
	if(battery < 20):
		$battery.text = "[color=\"#F44336\"]" + ("%.2fV" % battery) + "[/color]"
	elif (battery < 70):
		$battery.text = "[color=\"#FF9800\"]" + ("%.2fV" % battery) + "[/color]"
	else:
		$battery.text = "[color=\"#8BC34A\"]" + ("%.2fV" % battery) + "[/color]"

func _on_btn_exit_pressed():
	get_tree().root.propagate_notification(NOTIFICATION_WM_CLOSE_REQUEST)

func _on_btn_limpar_console_pressed():
	$console/container/log.text = ""

func _notification(what):
	if what == NOTIFICATION_WM_CLOSE_REQUEST:
		get_tree().quit() # default behavior

