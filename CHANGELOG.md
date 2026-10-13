# Historique des versions

## 2.6.1
- Skin NEON : le rose devient rouge (limiteur, haut des vumètres), comme dans les plugins Kinetik.

## 2.6.0
- Version « propre » pour les antivirus : l'ancien moteur intégré à Windows (APO) est entièrement retiré.
  KDYN-10 ne contient plus de DLL embarquée, ne demande plus jamais les droits administrateur,
  ne modifie plus le registre système et ne ferme plus d'autres programmes.
- « Désinstaller » rend le son à la sortie audio, retire le lancement au démarrage et efface les réglages.
- Fiche de version complète dans l'exe (éditeur, description, copyright).

## 2.5.0
- Dynamo devient **Kinetik KDYN-10** (nouveau nom, même logiciel) : logo Kinetik, nom dans la fenêtre, le menu et la zone de notification.
  Réglages, presets et mises à jour automatiques sont conservés.
- Licence propriétaire ; composants tiers mentionnés dans « À propos » et dans THIRD_PARTY_NOTICES.txt.

## 2.4.0
- Nouveau design dans la famille des plugins Kinetik / J-Slice : logo serif italique avec l'étoile à quatre branches,
  chiffres en Instrument Serif, étiquettes en IBM Plex Mono, potentiomètres à anneau de points, vumètres à LED.
- Quatre skins au choix (bouton SKIN en haut à droite, ou Réglages → Skin) : NEON (par défaut), MIDNIGHT, RISO, CLASSIC.
- Polices embarquées dans l'exe (SIL Open Font License), nouvelle icône.

## 2.3.0
- Mise à jour automatique et silencieuse depuis GitHub (vérification au démarrage puis toutes les 6 h, empreinte SHA-256 contrôlée, application à la fermeture de la fenêtre).
- Option « Mises à jour automatiques » et « Rechercher une mise à jour » dans les Réglages.

## 2.2.0
- Retrait du preset intégré « Bibite ».

## 2.1.0
- Suppression des restes des anciennes versions 1.x (moteur APO).

## 2.0.0
- Routage interne : Windows joue vers une sortie libre (ex. Analog 3+4), Dynamo traite et renvoie le son vers la sortie réelle.
