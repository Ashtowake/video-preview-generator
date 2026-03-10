import type { PropsWithChildren } from "react";
import { createContext, useContext, useMemo, useState } from "react";

import { locales, messages, type LocaleCode } from "./messages";

interface I18nContextValue {
  locale: LocaleCode;
  locales: LocaleCode[];
  copy: (typeof messages)[LocaleCode];
  setLocale: (locale: LocaleCode) => void;
}

const I18nContext = createContext<I18nContextValue | null>(null);
const STORAGE_KEY = "vpg-locale";

export const I18nProvider = ({ children }: PropsWithChildren) => {
  const [locale, setLocaleState] = useState<LocaleCode>(() => {
    if (typeof window === "undefined") {
      return "en";
    }

    const stored = window.localStorage.getItem(STORAGE_KEY);
    return locales.includes(stored as LocaleCode) ? (stored as LocaleCode) : "en";
  });

  const value = useMemo<I18nContextValue>(
    () => ({
      locale,
      locales,
      copy: messages[locale],
      setLocale: (nextLocale) => {
        window.localStorage.setItem(STORAGE_KEY, nextLocale);
        setLocaleState(nextLocale);
      },
    }),
    [locale],
  );

  return <I18nContext.Provider value={value}>{children}</I18nContext.Provider>;
};

export const useI18n = (): I18nContextValue => {
  const context = useContext(I18nContext);
  if (!context) {
    throw new Error("useI18n must be used inside I18nProvider");
  }

  return context;
};
