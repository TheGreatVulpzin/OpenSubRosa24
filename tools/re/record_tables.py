"""Record sizes and array capacities of the 024d globals.

The type headers in src/include are plain source: edit them directly. Ghidra scripts and the
storage test adapter read these two tables.
"""

# Record sizes (equal in the i386 Mach-O and in the 024d PE).
STRUCTS = {
    'Player': ('player', 0x2408),
    'Human': ('human', 0x2b64),
    'Item': ('item', 0x1ac),
    'Object': ('object', 0x8230),
    'Connection': ('connection', 0xf170),
    'ItemType': ('itemtype', 0x198),
    'ObjectType': ('objecttype', 0x828c),
    'Bond': ('bond', 0xec),
    'Event': ('event', 0x80),
    'Bullet': ('bullet', 0x5c),
    'Body': ('body', 0x40),
    # Match state, a single struct (not an array). Fields: analysis/structs/names.csv, and
    # analysis/structs/024d/game.csv (InferFields.java) for the rest.
    'Game': ('game', 0x25d64),
    # Regions with a known size and few or no known fields (Phase 4, batch 2). The size is
    # the symbol size of the Linux 0.24c server when the region has no pointer (the i386
    # Mach-O size has alignment padding), else the 32-bit size from the code.
    'Particle': ('particle', 0xf0),  # createparticle memsets 0xf0 bytes
    'Timer': ('timer', 0x58),
    'AreaBlock': ('area_block', 0x84),
    'Area': ('area', 0x100d6c),
    'Option': ('option', 0x150),
    'Server': ('server', 0x84),
    'Stats': ('stats', 0x8),
    'Config': ('config', 0x24),  # the client window settings that config.txt sets
    'SysInfo': ('sysinfo', 0x418),
    'SoundList': ('soundlist', 0x13c),
    'BlockList': ('blocklist', 0x40354),
    'City': ('city', 0xa0b99c),
    'MaterialList': ('materiallist', 0x580),
    'Intersect': ('intersect', 0x84),
    'TimerList': ('timerlist', 0xa0),  # 0x54 in Linux 0.24c; the 024d profiler uses up to +0x94
    'Speex': ('speex', 0x931000),
    'BsdSocket': ('bsdsocket', 0x20070),
    'Debug': ('debug', 0x523c8),
    'FileList': ('filelist', 0x80020),
    'Parser': ('parser', 0x10010c),
    # Phase 4, batch 3: tools/re/map_regions.py candidates whose Linux 0.24c size fits the
    # gap to the next known global (analysis/024d/manual_globals.csv has the evidence).
    'CityObjectType': ('cityobjecttype', 0x241800),
    'CityObject': ('cityobject', 0x15000),
    'MasterServer': ('masterserver', 0x208),
    'BuildBlockTypeRec': ('buildblocktype', 0x1a8),
    'BuildingType': ('buildingtype', 0x100d804),
    'Market': ('market', 0x204),
    'Scenario': ('scenario', 0x134),
    'Radix': ('radix', 0x400804),
    'GameWorld': ('game_world', 0x1c),
    # Phase 4, batch 4.
    'LevelEditor': ('level_editor', 0x1c6898),
    'BlockTemp': ('blocktemp', 0x7606c),
    'BondOpt': ('bondopt', 0x40),  # one record for each bond: 0x100000 / 0x40 = 16384
    'BondParticle': ('bondparticle', 0x20),
    'BondHuman': ('bondhuman', 0x810),
    'TempMesh': ('tempmesh', 0x2d1850),
    'Track': ('track', 0x578000),
    # Phase 4, batch 5.
    'Building': ('building', 0x36168f8),
    'AreaSector': ('area_sector', 0xc0d0000),
    # Phase 4, batch 6.
    'ObjectPacket': ('objectpacket', 0xce14),
}
# Capacity of each array (size of the global in the 0.24c ELF / record size).
COUNTS = {'player': 256, 'human': 512, 'item': 1024, 'object': 1024, 'connection': 256,
          'bond': 16384, 'bullet': 16384, 'event': 65536, 'particle': 16384,
          'objecttype': 128, 'bondopt': 16384, 'bondparticle': 16384, 'bondhuman': 512, 'itemtype': 64, 'body': 32, 'account': 65536,
          'buildblocktype': 16, 'area_block': 65536, 'area': 4, 'timer': 256, 'kickedip': 41368}
