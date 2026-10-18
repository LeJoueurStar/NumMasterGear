# NumMasterGear

Émulateur **Sega Master System** et **Game Gear** pour la calculatrice **NumWorks** (N0110, N0115, N0120).

Il se présente comme une application `.nwa` : tu envoies l'application et ta ROM sur [my.numworks.com/apps](https://my.numworks.com/apps), et le jeu se lance depuis le menu de la calculatrice.

## Fonctionnalités

- Fichiers `.sms` (Master System) et `.gg` (Game Gear), détectés automatiquement à partir de l'en-tête de la ROM.
- Affichage plein écran (par défaut) ou à la taille d'origine de la console.
- Vitesses x1, x2, x3, x4 et x1,5.
- Sauvegarde de l'état complet du jeu, qui fonctionne même pour les jeux sans pile de sauvegarde.
- Sauvegarde et sortie, sauvegarde sans quitter, effacement de la sauvegarde du jeu en cours.
- Réinitialisation du jeu.

## Commandes

| Touche | Action |
|---|---|
| Flèches | Croix directionnelle |
| OK | Bouton 1 |
| Retour | Bouton 2 |
| Retour arrière | Pause (Master System) ; Start, donc pause (Game Gear) |
| EXE | (non utilisée) |
| π | Réinitialiser le jeu |
| + / - | Remplir l'écran / taille d'origine |
| 1 / 2 / 3 / 4 / . | Vitesse x1 / x2 / x3 / x4 / x1,5 |
| √ | Sauvegarder sans quitter |
| 0 | Sauvegarder et quitter |
| Toolbox (maintenir ~1,5 s) | Effacer la sauvegarde du jeu en cours |
| Accueil | Quitter sans sauvegarder |

## Télécharger

Dernière version : [NumMasterGear.nwa](https://github.com/LeJoueurStar/NumMasterGear/releases/latest/download/NumMasterGear.nwa)

Puis installe-le sur [my.numworks.com/apps](https://my.numworks.com/apps) avec ta ROM (voir ci-dessous).

## Compilation

Il faut un environnement avec `arm-none-eabi-gcc`, `make`, `node` et `npm` (par exemple Debian sous Termux avec proot-distro).

```sh
make clean && make build
```

Le fichier produit est `output/NumMasterGear.nwa`.
## Installation

1. Ouvre [my.numworks.com/apps](https://my.numworks.com/apps) et envoie `NumMasterGear.nwa`.
2. Envoie aussi ta ROM (`.sms` ou `.gg`) comme fichier de données de l'application.
3. Installe l'application sur la calculatrice.

Si tu avais une ancienne version installée, réinitialise la calculatrice avant d'installer celle-ci.

## Limites connues

- Les cartouches coréennes (mappers spécifiques) ne sont pas entièrement prises en charge pour la restauration des sauvegardes.
- Les sauvegardes utilisent les enregistrements de fichiers d'Epsilon. Elles sont supprimées lors de l'installation d'une autre application, sauf si elles sont sauvegardées ailleurs.
- La calculatrice n'a pas de haut-parleur : le son n'est pas émulé.

## Légal

Ce projet ne contient aucune ROM de jeu. Tu dois utiliser tes propres cartouches ou des jeux libres.

## Licence

À définir (voir [CREDITS.md](CREDITS.md) pour les licences des composants repris).
