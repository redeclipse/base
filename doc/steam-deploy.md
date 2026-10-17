# Steam Deploy

Every `master` build that passes on Semaphore is deployed to the Steam **devel branch** automatically by the Deploy promotion (`src/semaphore/deploy.yml`, running `src/semaphore/deploy.sh steam`). Deploys run one at a time in the `steam-deploy` queue; a newer one replaces any still waiting.

The build account `redeclipsenet` is protected by the Steam Mobile Authenticator. CI holds the authenticator's `shared_secret` and generates each Steam Guard code itself, so nobody needs to hand over a code.

## Semaphore secret

The "Steam Token" secret provides:

| Variable | Contents |
| --- | --- |
| `STEAM_TOKEN` | The build account's password |
| `STEAM_SHARED_SECRET` | The authenticator's `shared_secret` (base64, as it appears in the maFile) |

Without `STEAM_SHARED_SECRET` the deploy only succeeds while steamcmd's cached login (`config/` beside steamcmd, kept in the Semaphore cache) is still valid.

## One-time setup

Do this on your own machine, not in CI.

1. **Install steamguard-cli** from the AUR (`steamguard-cli`), or grab a release from <https://github.com/dyc3/steamguard-cli>.
2. **Add the authenticator.** Run `steamguard setup` and log in as `redeclipsenet`. Steam emails a code to confirm, and may ask for a phone number on the account. If it does, add one to the account and run setup again.
3. **Save the revocation code.** Setup prints a revocation code (`R12345`). Put it in the password manager next to the account's password. It is the only way to remove the authenticator if the secret is lost.
4. **Get the shared secret.** steamguard-cli writes `~/.config/steamguard-cli/maFiles/redeclipsenet.maFile`. If you set a passkey during setup, run `steamguard decrypt` first. Copy the `shared_secret` value out of the maFile.
5. **Set the Semaphore secret.** Under Settings → Secrets → "Steam Token", add `STEAM_SHARED_SECRET` with that value, and delete the old `STEAM_GUARD` variable.
6. **Store the maFile, then delete it.** Put the maFile in the password manager, then remove `~/.config/steamguard-cli/maFiles/` so Semaphore holds the only live copy.
7. **Check it.** Start the Deploy promotion by hand once on the latest `master` workflow, and check that the log reaches the `SAVING CACHE` step without asking for a Steam Guard code.

## Generating a code by hand

To log into the build account somewhere else, restore the maFile and run `steamguard -u redeclipsenet`, or put the maFile in any app that reads Steam's `shared_secret`.

## If the secret leaks or the authenticator moves

- **Leaked:** change the build account's password, remove the authenticator with the revocation code (`steamguard remove`, or the revocation link on Steam's site), then do the setup again for a new `shared_secret`.
- **Moving to another authenticator app:** import the maFile there. The secret stays the same, so CI keeps working.
- **Re-running `steamguard setup` or adding the authenticator through the Steam app** makes a new `shared_secret`. Update `STEAM_SHARED_SECRET` straight away, or deploys fail at login.
