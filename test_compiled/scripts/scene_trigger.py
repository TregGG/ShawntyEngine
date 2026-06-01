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
        print(f"[SceneTrigger] Script started on: {self.entity_name}")

    def OnTriggerEnter(self, entity, other, input):
        """Called when another entity enters this trigger zone."""
        other_name = other.get_name()
        print(f"[SceneTrigger] '{other_name}' entered trigger '{self.entity_name}'")

    def OnTriggerExit(self, entity, other, input):
        """Called when another entity exits this trigger zone."""
        other_name = other.get_name()
        print(f"[SceneTrigger] '{other_name}' exited trigger '{self.entity_name}'")
