# braincheck

braincheck is a work-in-progress static and dynamic analyzer for the esoteric programming language [Brainfuck](https://en.wikipedia.org/wiki/Brainfuck).
It aims to provide users with a simple interface for verficiation of complex assertions about their program (reachability, values, over- and underflows).

![out](https://github.com/phagenlocher/braincheck/assets/15182576/fd07c5fc-ba27-4712-a5cb-cb399907af6b)

This software is built with [Spot](https://spot.lre.epita.fr), [CLI11](https://github.com/CLIUtils/CLI11) and [minunit](https://github.com/siu/minunit).

## Roadmap

- [ ] Better test coverage
- [ ] Improve performance by merging Kripke states
- [ ] Clean up `README.md`
- [ ] More assertions
  - [ ] Cell values
  - [ ] Cell visits
- [ ] Dynamic assertion checking with summary
