# Print outs from unit tests.

There are 323 bytes in each PES packet.
This is 1 + 7 x 46, so there should be 7 data blobs
in each PES packet.

### Test 1: descriptor_substreams_has_index_888_language_cat

ParseInternal pts=283413
1 dropping data_unit_id=2
2 packet 0 for pts=283413
3 packet 0 for pts=283413
4 packet 0 for pts=283413
5 packet 26 for pts=283413
6 packet 27 for pts=283413
7 packet 22 for pts=283413
Row 22 text Bon dia! alignment 3
Rows 1 pts=283413

### Test 2: EsParserTeletextTest.pes_283413_line_emitted_on_next_pes

ParseInternal pts=283413
1 dropping data_unit_id=2
2 packet 0 for pts=283413
3 packet 0 for pts=283413
4 packet 0 for pts=283413
5 packet 26 for pts=283413
6 packet 27 for pts=283413
7 packet 22 for pts=283413
Row 22 text Bon dia! alignment 3
Rows 1 pts=283413
ParseInternal pts=407876
1dropping data_unit_id=2
2 packet 0 for pts=407876
sending single line Bon dia! alignment 3
3 packet 0 for pts=407876
4 packet 0 for pts=407876
5 packet 26 for pts=407876
6 packet 27 for pts=407876
7 dropping data_unit_id=2

### Test 3: multiple_lines_with_same_pts

ParseInternal pts=8768632
1 dropping data_unit_id=2
2 dropping data_unit_id=2
3 dropping data_unit_id=2
4 dropping data_unit_id=2
5 dropping data_unit_id=2
6 packet 0 for pts=8768632
7 packet 0 for pts=8768632

ParseInternal pts=8773087
1 packet 26 for pts=8773087
2 packet 27 for pts=8773087
3 packet 20 for pts=8773087
Row 20 text -Sí? alignment 3
4 packet 22 for pts=8773087
Row 22 text -Sí. alignment 3
5 dropping data_unit_id=2
6 dropping data_unit_id=2
7 dropping data_unit_id=2
Rows 2 pts=8773087

ParseInternal pts=8937764
1 dropping data_unit_id=2
2 dropping data_unit_id=2
3 dropping data_unit_id=2
4 dropping data_unit_id=2
5 dropping data_unit_id=2
6 packet 0 for pts=8937764
7 packet 0 for pts=8937764

Note thee time differences

97.429s PTS=8768632 Packet 0 => signaled as start time
97.478s PTS=8773087 rows 20 & 22,  not used
99.308s PTS=8937764 packet 0 => end of row

There should thus be a two-row subtitle during 1.830s.


