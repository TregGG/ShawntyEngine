"""
SceneTrigger — attached to trigger zones in the scene.
Logs when entities enter or exit the trigger area.

Usage in scene JSON:
    "script": {
        "path": "test_compiled/scripts/scene_trigger.py",
        "class": "SceneTrigger"
    }
"""
import shawnty


class SceneTrigger:
    def OnStart(self, entity, input):
        """Called once when the scene loads."""
        self.entity_name = entity.get_name()
        self.players_in_trigger = set()
        # next_scene should be populated by JSON properties. Fallback to testscene2.scene.
        if not hasattr(self, "next_scene"):
            self.next_scene = "test_compiled/scenes/testscene2.scene"
        print(f"[SceneTrigger] Script started on: {self.entity_name}, target scene: {self.next_scene}")

    def OnTriggerEnter(self, entity, other, input):
        """Called when another entity enters this trigger zone."""
        if not other.is_alive():
            return
        
        category = other.get_category()
        other_id = other.get_id()
        other_name = other.get_name()
        
        print(f"[SceneTrigger] OnTriggerEnter: '{other_name}' (category: {category}, ID: {other_id})")
        
        if category == "Player":
            self.players_in_trigger.add(other_id)
            self.CheckTransition(entity)

    def OnTriggerExit(self, entity, other, input):
        """Called when another entity exits this trigger zone."""
        if not other.is_alive():
            return

        category = other.get_category()
        other_id = other.get_id()
        other_name = other.get_name()
        
        print(f"[SceneTrigger] OnTriggerExit: '{other_name}' (category: {category}, ID: {other_id})")
        
        if category == "Player" and other_id in self.players_in_trigger:
            self.players_in_trigger.remove(other_id)

    def CheckTransition(self, entity):
        # Transition logic should only execute on the server
        if shawnty.is_server():
            active_players = shawnty.get_active_players(entity)
            if not active_players:
                return

            print(f"[SceneTrigger] Checking transition: {len(self.players_in_trigger)} / {len(active_players)} players in trigger zone.")
            
            all_in = True
            for player in active_players:
                if player.get_id() not in self.players_in_trigger:
                    all_in = False
                    break
            
            if all_in:
                print(f"[SceneTrigger] Server: All active players in trigger. Transitioning to: {self.next_scene}")
                shawnty.change_scene(self.next_scene)
