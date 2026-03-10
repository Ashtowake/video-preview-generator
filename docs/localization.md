# Localization

## Goals

- Keep all user-facing copy in one place.
- Make adding a new language a data-entry task instead of a refactor.
- Keep route structure and page composition independent from the chosen locale.

## Current Structure

- Locale catalogs live in `apps/desktop/src/i18n/messages.ts`.
- Locale state lives in `apps/desktop/src/i18n/provider.tsx`.
- Components read translated copy from the provider instead of embedding strings directly.

## Adding a New Language

1. Add the locale code to the `locales` array.
2. Add a new catalog object with the same shape as `en` and `de`.
3. Add the locale label to the preferences UI if a human-readable name is needed.
4. Run the desktop type-check to catch missing message keys.

## Notes

- The current implementation localizes the desktop shell and editor copy.
- Project files remain language-neutral JSON so saved work is portable across locales.
