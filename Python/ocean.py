from os import system, name as osname
from random import random, choice
from time import sleep

REPRODUCE_TIME = 5
HUNGRY_LIVE_TIME = 4
MAX_TICKS = 100

CARNIVORE = 'C'
FISH = 'F'
WALL = '#'
EMPTY = '~'

RIGHT = (0, 1)
LEFT = (0, -1)
UP = (-1, 0)
DOWN = (1, 0)
DIRECTIONS = (RIGHT, UP, LEFT, DOWN)


class CellFiller(object):
    """Interface for any object in ocean"""

    filler_type = None

    def __init__(self, x, y, ocean=None):
        self.ocean = ocean
        self.x = x
        self.y = y
        self.had_moved = False

    def tick(self):
        pass


class MovableFiller(CellFiller):
    """Interface for any creature that can move and reproduce"""
    def __init__(self, x, y, ocean=None):
        super(MovableFiller, self).__init__(x, y, ocean)
        self.time_no_reproduce = 0
        self.ocean.creatures_count[self.filler_type] += 1

    def random_direction(self):
        return choice(DIRECTIONS)

    def try_move(self):
        pass

    def is_ready_to_reproduce(self):
        return self.time_no_reproduce > self.ocean.reproduce_time

    def move(self, x, y):
        self.ocean.clear_cell(self.x, self.y)
        self.ocean.ocean_map[x][y] = self
        self.x, self.y = x, y
        self.had_moved = True

    def get_moves(self, initial_direction=DIRECTIONS[0]):
        start_index = DIRECTIONS.index(initial_direction)
        for i in range(len(DIRECTIONS)):
            direction = DIRECTIONS[(start_index + i) % len(DIRECTIONS)]
            yield (self.x + direction[0], self.y + direction[1])

    def tick(self):
        if self.is_ready_to_reproduce():
            for x, y in self.get_moves(self.random_direction()):
                if (self.ocean.is_inside(x, y) and
                        self.ocean.ocean_map[x][y].filler_type == EMPTY):
                    self.time_no_reproduce = 0
                    self.ocean.ocean_map[x][y] = self.__class__(x, y,
                                                                self.ocean)
                    break

        self.time_no_reproduce += 1
        for x, y in self.get_moves(self.random_direction()):
            if self.ocean.is_inside(x, y) and self.try_move(x, y):
                break

        if isinstance(self, Carnivore):
            if self.must_die():
                self.die_of_hunger()
            else:
                self.count_hungry_steps += 1


class Carnivore(MovableFiller):
    filler_type = CARNIVORE
    count_hungry_steps = 0

    def random_fish_direction(self):
        for x, y in self.get_moves(self.random_direction()):
            if (self.ocean.is_inside(x, y) and
                    self.ocean.ocean_map[x][y].filler_type == FISH):
                return x - self.x, y - self.y
        return None

    def get_random_direction(self):
        # first, trying to find an eatable fish
        fish_direction = random_fish_direction()
        if fish_direction:
            return fish_direction
        return MovableFiller.get_random_direction()

    def try_move(self, x, y):
        current_item = self.ocean.ocean_map[x][y]
        if not (isinstance(current_item, Fish) or
                isinstance(current_item, EmptyCell)):
            return False

        if isinstance(current_item, Fish):
            self.ocean.creatures_count[FISH] -= 1
            self.count_hungry_steps = 0

        self.move(x, y)
        return True
            

    def must_die(self):
        return self.count_hungry_steps > self.ocean.hungry_live_time

    def die_of_hunger(self):
        self.ocean.creatures_count[CARNIVORE] -= 1
        self.ocean.clear_cell(self.x, self.y)


class Fish(MovableFiller):
    filler_type = FISH

    def try_move(self, x, y):
        current_item = self.ocean.ocean_map[x][y]
        if current_item.filler_type == EMPTY:
            self.move(x, y)
            return True
        else:
            return False


class Wall(CellFiller):
    filler_type = WALL


class EmptyCell(CellFiller):
    filler_type = EMPTY


ITEM_PROBABILITY = {Carnivore : 0.1, Fish : 0.1, Wall : 0.3}


class Ocean:
    def __init__(self, x, y, item_probability=ITEM_PROBABILITY,
                 reproduce_time=REPRODUCE_TIME, hungry_live_time=HUNGRY_LIVE_TIME):
        self.reproduce_time = reproduce_time
        self.hungry_live_time = hungry_live_time
        self.item_probability = item_probability
        self.max_x, self.max_y = x, y
        self.creatures_count = {CARNIVORE: 0, FISH: 0}
        self.ocean_map = [self.make_ocean_line(i, y) for i in range(x)]

    def __str__(self):
        return '\n'.join([''.join([item.filler_type for item in line])
                         for line in self.ocean_map]) + '\n'

    def random_item(self, x, y):
        random_part = random()
        for item in self.item_probability:
            if random_part < self.item_probability[item]:
                return item(x, y, self)
            random_part -= self.item_probability[item]
        return EmptyCell(x, y)

    def make_ocean_line(self, line_index, line_size):
        return [self.random_item(line_index, i) for i in range(line_size)]

    def is_inside(self, x, y):
        return (0 <= x < self.max_x and 0 <= y < self.max_y)

    def clear_cell(self, x, y):
        self.ocean_map[x][y] = EmptyCell(x, y)

    def update(self):
        for i in range(self.max_x):
            for j in range(self.max_y):
                self.ocean_map[i][j].had_moved = False

    def next_tick(self):
        self.update()

        for i in range(self.max_x):
            for j in range(self.max_y):
                item = self.ocean_map[i][j]
                if not item.had_moved:
                    item.tick()


def run(x, y, visualize=False, ticks_number=MAX_TICKS):
    ocean = Ocean(x, y)
    tick_index = 0
    while (tick_index < ticks_number and
           ocean.creatures_count[CARNIVORE] > 0 and
           ocean.creatures_count[FISH] > 0):
        if visualize:
            # if osname == 'posix':
            #     system('clear')
            # else:
            #     system('cls')
            print(ocean)
        ocean.next_tick()
        tick_index += 1

run(20, 20, visualize=True)
