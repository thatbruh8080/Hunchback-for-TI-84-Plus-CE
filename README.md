# Hunchback-for-TI-84-Plus-CE
Arcade conversion of the 1983 coin-op game of the same name, made with the CE C Toolchain. So Hunchback in math class wooo

The coin op original was made by Century Electronics, in 1983
<div>
  <img src="scrshot1.png" alt="title" title="title"/>
</div>
<div>
  <img src="scrshot2.png" alt="openg" title="openg"/>
</div>
<div>
  <img src="scrshot3.png" alt="rope" title="rope"/>
</div>

Basically you're this not so handsome lad with sciliocis called Quasimodo who has to save the fair princess Esmeralda (I thought she was a French Roma dancer girl) through Nottingham Castle. 
Along the way you have to jump over arrows and fireballs, do some Pitfall-swinging over burning pits, jump over ramparts, some of those ramparts containing knights with lances ready to skewer your butt, four bells you have to jump on and at the end of each screen there's a church bell on the right that you have to ring to get to the next stage. There's 32 stages I believe, every sixteen stages you go into a Rivets-like stage, like the one in Donkey Kong where you have to walk through bell platform thingies to make em disappear before every Esmeralda disappears (Yes there are five Esmeraldas before they become heartbroken every 24.3 seconds). Beat those 32 stages and the game loops and becomes harder

You get a **BONUS MAN AT 10000**

# Controls
`Left and right on D-keys` Moves Quasi (obviously)

`2nd` Jump

`5` inserts coin

`1` starts one player game if there's credits (Ah crud forgot to implement two player mode atm)

`clear` quits

`Typing in "ISUCK"` activates Developer Mode, you can choose levels and choose lives up until ten then infinity. You can kinda say it's a cheat mode but scores are not saved after a game over

# erro
Unless you're below OS 5.5.5 TI got rid of machine language program support (I mean this is OBVIOUSLY not made using TI-Basic) so you need a jailbreak called ArTIfiCE, on current versions there's a program called A, that's the jailbreak.

But even then you need clibs because the game was written using the CE C Toolchain and utilizes special libraries such as grafx and such. Have both and you're good to go
