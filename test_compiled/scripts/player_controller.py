"""
PlayerController — example script for player movement.
Shows how to read input, modify velocity, and apply impulses.

Usage in scene JSON:
    "script": {
        "path": "test_compiled/scripts/player_controller.py",
        "class": "PlayerController",
        "properties": {
            "move_speed": 5.0,
            "jump_force": 18.0
        }
    }
"""
import shawnty


class PlayerController:
    # Default values — can be overridden by scene JSON properties
    move_speed = 5.0
    jump_force = 18.0

    def OnStart(self, entity, input):
        """Called once when the scene loads."""
        name = entity.get_name()
        print(f"[PlayerController] Attached to '{name}' | speed={self.move_speed} jump={self.jump_force}")

    def OnUpdate(self, entity, dt, input):
        """Called every frame. Handles WASD movement and jumping."""
        rb = entity.get_rigidbody()
        if rb is None:
            return

        # Horizontal movement
        move_x = 0.0
        if input.is_key_down(shawnty.KEY_A):
            move_x -= self.move_speed
        if input.is_key_down(shawnty.KEY_D):
            move_x += self.move_speed

        # Set horizontal velocity, preserve vertical
        vel = rb.velocity
        rb.velocity = shawnty.Vec2(move_x, vel.y)

        # Jump on space press
        if input.is_key_pressed(shawnty.KEY_SPACE):
            rb.apply_impulse(shawnty.Vec2(0.0, self.jump_force))

    def OnTriggerEnter(self, entity, other, input):
        """Called when this player enters a trigger zone."""
        print(f"[PlayerController] Touched trigger: {other.get_name()}")

    def OnDestroy(self, entity):
        """Called when the entity is destroyed."""
        print(f"[PlayerController] Destroyed: {entity.get_name()}")
