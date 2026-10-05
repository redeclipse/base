# SDD ledger — plan: doc/superpowers/plans/2026-10-02-geometry-templates.md

Spec: doc/superpowers/specs/2026-10-02-geometry-templates-design.md (read; "Revisions during planning" overrides earlier sections)

## Pre-flight scan

| Pair / task | Produces → consumes | Finding |
|---|---|---|
| T2 → T3 | geomtemplatebox, capturegeomtemplate, calcgeominstance(bb), geominstancescale → buildgeomtemplate, sync, geominstancebb | consistent signatures |
| T3 → T5 | geominstancetemplate, geominstancebb, removeoctaentity → modifyoctaentity, getentboundingbox, entselectionbox | consistent |
| T3 → T11 | geomtemplate::bih (NULL, freed via DELETEP) → buildgeomtemplatebih, collision | consistent |
| T3 ↔ T4 | commitchanges (T3) / changed() + markgeomtemplates (T4), both octaedit.cpp | T4 adds to changed(), T3 rewrites commitchanges: disjoint functions |
| T3 ↔ T8 | octaedit.cpp edfillsel (T3) / rendergeomtemplateboxes call (T8) | disjoint |
| T5 → T7/T9 | octaentities.instances, instquery, va->instances → findvisibleinstances, findshadowinstances | consistent |
| T6 → T7/T9/T10 | ATTRIB_INSTANCE*, resetinstance, glDrawElementsInstanced_, glVertexAttribDivisor_, shader includes → bind/unbind, instanced draws | consistent |
| T7 → T9/T10 | prepareinstances(const vector<int>&), bindinstances, unbindinstances, renderinstancegroups, geominstancevisible, instgroups → shadow/RSM/mesh | T9 placement: shadowinsts + forward decls above rendershadowmapworld; definitions after T7 section (plan text corrected before execution) |
| T9 → T10 | shadowinsts declared above rendershadowmapworld → genshadowmesh (later in file) | consistent |
| T1 → T3 | entinfo cases (T1) replaced by extended versions (T3) | consistent |
| selftest | each task inserts before the "later tasks" marker; $script:T1/I1/I2/I7 carried | T5 refills the block after T4 edits; T7 uses I1 (T5); T9 uses I1, I2; consistent |
| T1 self | step 1 passes pre-change, step 2 fails pre-change | ok |
| T2 self | tests reference only Task 2 functions + engine macros | ok |
| T3 self | tests need edfillsel + geotemplateinfo, both in task | ok |
| T4 self | ok |
| T5 self | geoinstancebb command in task | ok |
| T6 self | baseline recorded before edits | ok |
| T7 self | geoinststats in task | ok |
| T8 self | review-only step (screenshot) | ok — visual check is the test |
| T9 self | rhprobe may need GI enabled; plan says adjust setup | ok |
| T10 self | ok |
| T11 self | ok |
| T12 self | docs | ok |

Ruling: work on branch `geometry-templates` in the main checkout, not a worktree — build.sh, bin/amd64 (DLLs) and the harness home are tied to the repo root — cost if wrong: unrelated untracked files in the user's tree stay in place; implementers must `git add` only the files their task lists.
Ruling: spec and plan (doc/, untracked) stay uncommitted — CLAUDE.md says commit only when asked; the user approved per-task feature commits by choosing the plan — cost if wrong: user commits the docs themselves.

## Tasks
Task 1: dispatched (base fd99febc, implementer sonnet)
Task 1: implementer DONE_WITH_CONCERNS (34b35e5c); review dispatched
Ruling: geotemplate entities read back with 5 attrs (`geotemplate 1 16 16 16 0`) because entities::numattrs pads unused types to 5 (entities.cpp:801); self-test expectation adjusted — matches existing engine behaviour, attribute 4 is unused — cost if wrong: none functional; later geot_get expectations must include the trailing 0.
Ruling: Task 1 panel check accepted via the panel's own setter (tool_ent_attr_change) since harness arrow clicks don't work for any entity panel (physics too) — cost if wrong: a real-mouse panel bug goes unseen until manual use.
Task 1: review — spec ✅, approved; Important #1 (test pins padded 5th attr) is the plan-conflict already ruled above.
Ruling: keep the `geotemplate 1 16 16 16 0` expectation — it is the engine's real entget output, and asserting it catches attr-count regressions — cost if wrong: one test string to update if numattrs padding changes.
Task 1: minor (deferred): self-test header/variables name later-task commands before they exist (plan-mandated scaffolding)
Task 1: minor (deferred): self-test leaves home/uitest maps/harness_geot.mpz behind
Task 1: minor (deferred): no editor icons textures/icons/edit/geotemplate.png / geoinstance.png (falls back to "?", like worldcol)
Task 1: minor (deferred): geoinstance listed in tool_ent_types_env though it has modes/muts like the game-list physics/worldcol
Task 1: ⚠️ resolved — entities::isallowed reads modesattr/mvattr generically from enttype (entities.cpp:803-810)
Task 1: complete (commits fd99febc..34b35e5c, review clean)
Task 2: dispatched (base 34b35e5c, implementer sonnet)
User note (mid-run): editor numinputs are Blender-style — double-click to type, or hold LMB and drag left/right. Task 1's "arrow clicks do nothing" was wrong usage, not a harness limit.
Ruling: re-verify both entity panels through the real input path (double-click + type, and drag) as part of Task 12's final verification — cost if wrong: a panel bug found later than it could have been.
Task 2: implementer DONE_WITH_CONCERNS (dc800f3d); unit-test pass signal is a clean harness start (conoutf runs before the log opens); sabotage run overwrote the untracked redeclipse-crash.dmp test artifact; build via PowerShell wsl, not Git Bash
Task 2: minor (deferred): unit tests don't pin pitch/roll direction or order (length-only check)
Task 2: minor (deferred): capture tests lack deeper octree descent, size-32 leaves, vertinfo content check
Task 2: minor (deferred): bottom-layer-only faces render the top texture in templates (plan-mandated blend strip) — check visually
Task 2: minor (deferred): cubeinsidebox/cubeoverlapsbox local helpers
Task 2: complete (commits 34b35e5c..dc800f3d, review clean)
Task 3: dispatched (base dc800f3d, implementer sonnet)
Task 3: implementer DONE (2608a73b); deviations: brace solidfaces/emptyfaces macros, split comma decl in ICOMMAND, template VA fns placed after updateva
Task 3: minor (deferred): geotemplatestate uses va->tris only — alpha-only template reads "empty"/"missing" though geominstancetemplate returns it
Task 3: minor (deferred): duplicate-id warning repeats on every sync
Task 3: minor (deferred): self-test lacks instance count / resetgl / duplicate-warning coverage
Task 3: minor (deferred): flushvbo at buildtemplatevas start (plan-mandated; harmless — pending data empty at both call sites) — add a comment
Task 3: minor (deferred): recalcprogress not zeroed before template updateva (stale progress fraction)
Task 3: minor (deferred): destroytemplateva duplicates destroyva's freeing half; csi calc duplicated
Task 3: complete (commits dc800f3d..2608a73b, review clean)
Task 4: dispatched (base 2608a73b, implementer haiku)
Task 4: implementer DONE_WITH_CONCERNS (9f26587a) — undo does not rebuild templates, 2 self-test checks fail; escalating fix to sonnet (haiku judged it out of scope)
Task 4: fix (13599570) — root cause: undo refused by noedit 'Selection not in view' (camera faced away); test now frames selection before undo; no engine change
Task 4: minor (deferred): self-test step 8 (edit outside) can pass vacuously — doesn't prove the edit happened
Task 4: minor (deferred): overlap step doesn't assert the edited cube changed tris
Task 4: minor (deferred): change box grown by 1 twice in the block3 path (plan-mandated, conservative)
Task 4: complete (commits 2608a73b..13599570, review clean)
Task 5: dispatched (base 13599570, implementer sonnet)
Task 5: implementer DONE (1f099bf1); added entselintersect(instances) to physics.cpp disttoent early (plan had it in Task 11); flagged getshadowvabb ignores va->instances
Ruling: accept entselintersect(instances) in Task 5 — hover/selection of instances needs it now; Task 11's brief must not add it again — cost if wrong: none.
Ruling: Task 9 must also make getshadowvabb use the full bb when va->instances is non-empty (renderva.cpp ~1131), else instances outside a vertex array's geometry bounds drop out of shadow culling — cost if wrong: missing shadows near VA edges.
Task 5: review — spec ✅, Needs fixes: 2 Important (plan-mandated)
Ruling: union the geoinstance octree bounds with e.o ± entselradius in getentboundingbox (mapmodel/decal precedent) — the plan's bounds left instances with a pivot outside their geometry unpickable; geoinstancebb (render/test bounds) stays the pure transformed capture box — cost if wrong: slightly larger culling/node bounds.
Ruling: call recalcoctaentbb in the mapmodel and decal remove cases too — the per-list recomputation drops other lists' bounds once instances share a node — cost if wrong: a behaviour change in existing mapmodel/decal bookkeeping (strictly more correct bounds).
Task 5: minor (deferred): hover self-test doesn't distinguish oe.instances vs oe.other; no stale-entry check; step name overstates
Task 5: minor (deferred): full instance selection box lacks es.max(entselradius) and may not enclose e.o
Task 5: minor (deferred): remove path dirties va bb even when instance was only in `other`
Task 5: minor (deferred): findents scans only oe.other
Task 5: fix round 1 dispatched (resume implementer; FIX_BASE 1f099bf1)
Task 5: fix round 1/5 (2 addressed, 0 open; commits 1f099bf1..802a2a7d)
Task 5: complete (commits 13599570..802a2a7d, review clean)
Task 6: dispatched (base 802a2a7d, implementer sonnet)
Task 6: implementer DONE_WITH_CONCERNS (9817b875); shaders.ps1: 818 PASS-TEXT, 622 PASS-PIXEL (all world), 1 WEAK (smworld depth-only), 0 FAIL after working around contract tier
HARNESS ISSUE (for the user, not fixed): shaders.ps1 `record -Sids s00` baseline can't be compared by `check` — record resets sweep vars to engine defaults, check leaves live values (gdepthformat live 1 vs default 0) → 1034 spurious FAILs with no shader change. Implementer recorded under check's conditions instead; harness scripts untouched.
Ruling: accept the 4-line shaderharness.cpp bench change (feeds identity/1 to vinstance* attributes) — without it random attribute data makes every world shader's pixel tier meaningless — cost if wrong: bench no longer exercises non-identity instance transforms (that's Task 7's job via real rendering).
Ruling: shadowmap.vert (alpha/mask shadow variants) stays untouched — instances never draw alpha faces in v1 — cost if wrong: none until alpha instances (stretch goal).
Task 6: review — spec ✅, approved; Important #1 (plan-mandated): blendmap texcoord1 uses wpos
Ruling: keep texcoord1 from wpos — template builds strip LAYER_BOTTOM, so template vertex arrays have no blend batches and instances never draw a WORLD_BLEND/RSM_BLEND variant; for the world wpos == vvertex bit for bit — cost if wrong: revisit (switch to vvertex) if blend layers on instances (stretch goal) are ever added.
Task 6: minor (deferred): INSTANCE_POS uses v.w unparenthesised / evaluates v 4x
Task 6: minor (deferred): shader bench draws with slots 10-13 enabled then doesn't call gle::resetinstance() afterwards
Task 6: minor (deferred): smworld change only WEAK in shader harness (depth-only) — covered by Task 9 shadow screenshots
Task 6: minor (deferred): no null check on glDrawElementsInstanced_/glVertexAttribDivisor_ (matches other core pointers)
Task 6: note for Task 7+: Z-pass (nocolor), depth, ldrnotexture, bbquery, blendbrush shaders don't read the instance transform — instances must not go through those passes
Task 6: complete (commits 802a2a7d..9817b875, review clean)
Task 7: dispatched (base 9817b875, implementer sonnet)
Task 7: implementer DONE (b20a4903); 2 instances/24 tris drawn, 0 looking at sky; screenshot checked by controller: both instances render textured
Task 7: review — spec ✅, Needs fixes: 2 Important (plan-mandated)
Ruling: issue the instance bounding-box queries BEFORE drawing the instances (queryinstances then renderinstancegroups), so each query tests world depth only — drawing first lets a tight instance occlude its own query (flicker) — cost if wrong: one frame of extra latency in un-occluding, same as mapmodels' box queries.
Ruling: Important #2 (bottom-layer blend batches drawn darkened) is unreachable for templates — copytemplatecube strips LAYER_BOTTOM to LAYER_TOP and addmerge builds merged surfaces from brightsurface (octa.cpp:1676), so template VAs carry no LAYER_BOTTOM elements — park — cost if wrong: darkened patches on instances of blend-painted sources; would show in a blend-painted screenshot.
Task 7: minor (deferred): cleanupinstances doesn't clear instgroups/instdata (stale template pointers until next prepare)
Task 7: minor (deferred): geoinststats counts t->tris (opaque range is drawn) and is overwritten by drawtex passes
Task 7: minor (deferred): glBufferSubData append may sync on some drivers (map-unsynchronized idiom)
Task 7: minor (deferred): oe->instquery shared across OQStates (same as mapmodels' oe->query)
Task 7: fix round 1 dispatched (resume implementer; FIX_BASE b20a4903)
Task 7: fix round 1/5 (1 addressed, 0 open; commits b20a4903..0031e123)
Task 7: minor (deferred): occlusion step 17 may pass via world VA occlusion, not instquery itself
Task 7: complete (commits 9817b875..0031e123, 1 parked: blend-layer finding ruled unreachable)
Task 8: dispatched (base 0031e123, implementer haiku)
Task 8: implementer DONE (a85e8211)
Task 8: review — ❌ captured box invisible (coplanar with block faces, drawn before line polygon offset at octaedit.cpp ~675); screenshot-only test; implementer report claimed it was visible
Task 8: minor (deferred): comment should name the GL state; 48,48,48 additive may wash out on bright scenes
Task 8: fix round 1 dispatched (resume implementer; FIX_BASE a85e8211)
Task 8: fix round 1/5 (1 addressed, 1 open — pixel test matches ground/sky (33k px), claimed RED implausible; commits a85e8211..0d82dbc1)
Ruling: round 2 goes to a fresh sonnet implementer instead of resuming haiku — the haiku implementer reported unverified visual/RED evidence twice — cost if wrong: one extra cold-start.
Task 8: fix round 2 dispatched (FIX_BASE 0d82dbc1, sonnet)
Task 8: fix round 2/5 (1 addressed, 0 open; commits 0d82dbc1..6bf1c901)
Task 8: minor (deferred): pixel-test rectangle/thresholds tied to 1280x720 + fixed camera
Task 8: complete (commits 0031e123..6bf1c901, review clean)
Task 9: dispatched (base 6bf1c901, implementer sonnet)
Task 9: implementer DONE (b06bdf87); sun/point pixel checks vs no-shadow controls; rhprobe differs slightly (0.1-0.4%)
Task 9: minor (deferred): GI assertion weak (a -ne b; deltas <0.4%); second rhprobe count unchecked
Task 9: minor (deferred): rendershadowinstances re-sorts/re-uploads per cube side (perf)
Task 9: minor (deferred): xtravertsva units (indices vs verts) inconsistent
Task 9: minor (deferred): sun step leaves sunlightpitch/yaw changed
Task 9: minor (deferred): no SM_SPOT instance shadow test
Task 9: note (existing engine, not this feature): findshadowvas cube path `smbbcull ? 0x3F : calcbbsidemask` looks inverted vs spot path `!smbbcull || ...` — mirrored as spec required; flag to user
Task 9: complete (commits 6bf1c901..b06bdf87, review clean)
Task 10: dispatched (base b06bdf87, implementer sonnet)
Task 10: implementer DONE_WITH_CONCERNS (e24ba7ba engine fix + 2daa9fb9 baking); mesh-vs-live: 2.85% without baking (FAIL), 0.08% with (PASS)
Ruling: keep e24ba7ba (allchanged builds shadow meshes after clearshadowcache) as its own commit — without it the cached-mesh path never runs (clearshadowcache cleared the meshes right after genshadowmeshes), so Task 10 would be untestable dead code — cost if wrong: every map's static point/spot lights switch to the cached-mesh shadow path (perf/visual change); revert that one commit. FLAG TO USER.
Ruling: accept geot_find helper + refreshed $script:I1/I2 — reloaded maps reorder entities by type, so stored indices go stale after a save/reload — cost if wrong: none.
Task 10: review — spec ✅, Needs fixes: 3 Important
Ruling (supersedes the e24ba7ba ruling above): revert e24ba7ba so every map keeps master's shadow behaviour; keep the instance-baking code (correct, latent while master discards meshes at load); verify it through a DEBUG_UTILS test command that regenerates shadow meshes on demand (refused under IDF_MAP) plus a mesh-count query — enabling the dormant mesh path globally would expose a stale-shadow bug for trigger/hide mapmodels (genshadowmeshmapmodels bakes them once at load) and raise GPU memory on every map; that is the user's decision, not this feature's — cost if wrong: instance baking stays dormant in normal play until the user enables the mesh path (and fixes mapmodel baking). FLAG TO USER: master's cached shadow-mesh path is dead (allchanged clears meshes right after building them).
Ruling: remove the leftover DIAGPRE geot_dump debug lines from the self-test; add an assertion that meshes exist (>0) before the mesh-on shot and none (0) before the mesh-off shot.
Task 10: minor (deferred): Compare-Shots takes first recursive filename match, no size check, 1% threshold ~3.5x below RED
Task 10: minor (deferred): harness_geot map saved with sunlight 0 and L2
Task 10: fix round 1 dispatched (resume implementer; FIX_BASE 2daa9fb9)
Task 10: fix round 1/5 (3 addressed, 0 open; commits 2daa9fb9..9e6d2fd4) — e24ba7ba reverted by c325ec64; test commands edgenshadowmeshes/edshadowmeshcount added
Task 10: complete (commits b06bdf87..9e6d2fd4, review clean)
Task 11: dispatched (base 9e6d2fd4, implementer sonnet)
User observation (Task 11, live): projectiles collide with instances about as well as with mapmodels, and stains work. Known engine issue: projectiles sometimes pass through BIHs at certain angles (mapmodels too) — pre-existing, not a Task 11 defect.
Task 11: implementer DONE_WITH_CONCERNS (1d73be52); fixed two shared BIH::traverse bugs (zero ray component ordering; node-boundary face pruning) — affects mapmodels; edraycast uses radius worldsize*2 for a -1 miss; walk test: blocked at face, no-collide passes through; stain seen (no negative control)
FLAG TO USER: BIH::traverse fixes may relate to the known projectile pass-through on mapmodels.
Task 11: review — spec ✅ (both BIH::traverse fixes judged correct for all callers; horizontal mapmodel rays now hit), Needs fixes: 2 Important
Ruling: fix both in shared bih.cpp, in their own commit(s) with messages that say mapmodels are affected — (1) BIH::traverse must return the nearest hit across meshes (track min dist + hitsurface, shrink maxdist) since template BIHs routinely have one mesh per vertex array; (2) entradius from the farthest corner (per-axis max |bbmin|,|bbmax|) since template content can be lopsided about the pivot — both are strict correctness gains for mapmodels — cost if wrong: gameplay-visible raycast changes on mapmodels (more correct hits); FLAG TO USER.
Task 11: minor (deferred): alpha-material template faces excluded from BIH (non-solid), per spec "opaque triangles"
Task 11: minor (deferred): geninsttris doesn't skip EF_NOVIS/!isallowed (mirrors genmmtris)
Task 11: minor (deferred): rays tested at yaw only; COLLIDE_OBB path untested; $t7 unused / template 7 left in map
Task 11: fix round 1 dispatched (resume implementer; FIX_BASE 1d73be52)
Task 11: fix (af4a4ccb bih nearest-hit + entradius, 28dae4e4 tests; 24 self-test steps pass); scoped re-review dispatched (FIX_BASE 1d73be52, head 28dae4e4) — PAUSED HERE on usage limit: on resume, re-run that re-review if its result is lost, then complete Task 11, then Task 12 (docs + real-input panel check), then the final whole-branch review.
Task 11: fix round 1/5 (2 addressed, 0 open; commits 1d73be52..28dae4e4)
Task 11: minor (deferred): equal-t hits now last-wins for hitsurface; overflow-path descends far child needlessly
Task 11: minor (deferred, pre-existing): disttoent passes radius not current best dist to mmintersect/geominstanceintersect → farther entity hit can overwrite global hitsurface
Task 11: complete (commits 9e6d2fd4..28dae4e4, review clean)
Task 12: dispatched (base 28dae4e4, implementer sonnet)
Task 12: implementer DONE (445c545a); 3 self-tests pass; panel real-input check PASSED on both panels (double-click+type, drag)
HARNESS ISSUE (for the user, not fixed): uikeypress can't type digits (implementer posted WM_CHAR); uisetcursor can't drive numinput drags — engine reads OS mouse motion; Win32 SendInput works but moves the real pointer and needs the window foregrounded; posted WM_MOUSEMOVE unreliable.
Task 12: minor (deferred): selftest header lists 5 of 7 commands; spec test-command list lacks the 2 shadow-mesh commands; README could say edgenshadowmeshes returns 0 when smmesh 0; CLAUDE.md geoinstancebb empty cases incomplete
Ruling: accept that Task 12's commit adds the spec file to git (the plan's commit step listed it) — supersedes the earlier "docs stay uncommitted" ruling for the spec only; the plan stays untracked — cost if wrong: user removes one file from the branch.
Task 12: complete (commits 28dae4e4..445c545a, review clean)
Final review: dispatched (fd99febc..HEAD, opus)
Final review: With fixes — no correctness bugs; Important: (1) self-test lacks lifecycle coverage with live instances (template move/re-id/delete, resetgl, map switch), (2) pitch/roll untested end to end (incl. COLLIDE_OBB)
Ruling: one final fix dispatch covering Important 1+2, the shaderharness gle::resetinstance() one-liner, the stale engine.h comment, and refusing edfillsel under multiplayer — cheap, real risk — cost if wrong: none.
Ruling: leave history rewriting (squash e24ba7ba/c325ec64, fix trailers on a85e8211/0d82dbc1) to the user at finishing — rewriting commits is the user's call — cost if wrong: two noise commits in history.
Ruling: disttoent radius→dist and the smbbcull inversion stay out of this branch (pre-existing, change mapmodel behaviour) — flag to user as separate fixes — cost if wrong: known mapmodel quirks remain.
Final review: deferred (not blocking): duplicate stains per overlapping node (also mapmodels); per-pass CPU costs (findgeomtemplate linear, matrices recomputed); VERSION_GAME not bumped for entity renumbering (same as worldcol precedent) — release coordination; active-edit mode doesn't rebuild until commit; BIH short overflow for >32767 from pivot.
Final fix wave: dispatched (FIX_BASE 445c545a, sonnet)
Final fix wave: DONE (bcaf48e3, cb189ca1, 3c7796d6, 15f2bf16); found+fixed pre-existing raycube hitents bug (hover left hitents populated → later raycube skipped that entity); edcollide test command added; FLAG TO USER
Final fix wave re-review: all 5 findings addressed, no new Critical/Important; note: hitents fix affects gameplay rays too (projectile barrier, hitscan, LOS) — plausible root cause of the known BIH pass-through
Final: complete (commits fd99febc..15f2bf16)
