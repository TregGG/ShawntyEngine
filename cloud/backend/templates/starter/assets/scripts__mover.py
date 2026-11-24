"""Moves an entity back and forth. Scripts run on the server; every player sees the result."""
import math
import shawnty


class Mover:
    distance = 3.0
    speed = 1.0

    def OnStart(self, entity, input):
        self.start = entity.get_transform().position
        self.time = 0.0

    def OnUpdate(self, entity, dt, input):
        self.time += dt
        offset = math.sin(self.time * self.speed) * self.distance
        entity.get_transform().position = shawnty.Vec2(self.start.x + offset, self.start.y)
