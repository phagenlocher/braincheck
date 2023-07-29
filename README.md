# braincheck

braincheck is a work-in-progress static and dynamic analyzer for the esoteric programming language [Brainfuck](https://en.wikipedia.org/wiki/Brainfuck).
It aims to provide users with a simple interface for verification of complex assertions about their program (reachability, values, over- and underflows).

![out](https://github.com/phagenlocher/braincheck/assets/15182576/513ba1a2-6f85-45b0-af90-d338b70f604f)

This software is built with [Spot](https://spot.lre.epita.fr), [CLI11](https://github.com/CLIUtils/CLI11) and [minunit](https://github.com/siu/minunit).

## Roadmap

- [ ] Better test coverage
- [ ] Improve performance by merging Kripke states
- [ ] Clean up `README.md`
- [ ] More assertions
  - [ ] Cell values
  - [ ] Cell visits
- [ ] Dynamic assertion checking with summary
