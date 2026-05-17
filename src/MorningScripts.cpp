#include "MorningScripts.h"

#include <ctime>
#include <random>

const MorningScript& pickRandomMorningScript() {
    // Twelve hardcoded themed scripts. Each is internally coherent — its beats
    // reference each other and never bleed into another script. House dominates
    // every scene (most lines, meanest takes). Every script closes on a House
    // line so the rhythm of the ending is consistent.
    static const std::vector<MorningScript> pool = {

        // Script 1 — Clinic-hours power move
        {
            {Speaker::Cuddy,   "House. My office. Ten minutes. Don't pretend you didn't see the email."},
            {Speaker::House,   "Pass. You'll come to me. Bring coffee."},
            {Speaker::Wilson,  "You parked in the OB-GYN lot. Again."},
            {Speaker::House,   "Only spot Cuddy didn't park her Lexus diagonally in."},
            {Speaker::Cuddy,   "Your clinic hours this month: zero. That's actively impressive."},
            {Speaker::Foreman, "New case file's on your desk. Try opening it before noon."},
            {Speaker::House,   "Foreman. Tell me it's not Sarcoidosis."},
            {Speaker::Foreman, "Pay up. Thirty bucks. The kid had it. I called it."},
            {Speaker::House,   "I'm not paying you to diagnose the most common thing in the building."},
            {Speaker::Wilson,  "Lunch at noon. Your turn to buy. For once in a calendar year."},
            {Speaker::House,   "Two emails, three phone calls, a Cuddy, and a Wilson before nine. Tuesday's going great."},
        },

        // Script 2 — House is late, blames Cuddy's parking
        {
            {Speaker::Wilson,  "You're forty minutes late. Even for you that's ambitious."},
            {Speaker::House,   "Cuddy's Lexus is in three spots. I had to walk from the cemetery."},
            {Speaker::Cuddy,   "My Lexus is in ONE spot. You drove around it on purpose."},
            {Speaker::House,   "I drove around the principle of it. The car was incidental."},
            {Speaker::Foreman, "Differential meeting was at eight. We started without you."},
            {Speaker::House,   "Foreman, you can't diagnose anything without me. The board's blank."},
            {Speaker::Foreman, "It's not blank. We just erased your wrong answers from yesterday."},
            {Speaker::Wilson,  "He's not wrong. Yesterday was rough."},
            {Speaker::House,   "Yesterday I was right. The patient was wrong. There's a difference."},
        },

        // Script 3 — Wilson's latest divorce just settled
        {
            {Speaker::Cuddy,   "Wilson. The papers came through?"},
            {Speaker::Wilson,  "Final signature yesterday. I'm officially single again."},
            {Speaker::House,   "Third one. You're collecting them like trading cards."},
            {Speaker::Wilson,  "I have two ex-wives, House. Two."},
            {Speaker::House,   "And one almost-fiancee who left with the dog. Don't sell yourself short."},
            {Speaker::Cuddy,   "Be nice. It's been a hard week for him."},
            {Speaker::House,   "Every week is a hard week for him. He's a cancer doctor with a saint complex."},
            {Speaker::Wilson,  "Are you done?"},
            {Speaker::House,   "Almost. The dog took the better half, by the way."},
            {Speaker::Foreman, "I'm just here for the show. Don't mind me."},
            {Speaker::House,   "Wilson. Lunch on me. I'll insult you with cake."},
        },

        // Script 4 — Vending machine ate Foreman's dollar
        {
            {Speaker::Foreman, "The vending machine ate my dollar. Third time this week."},
            {Speaker::House,   "Foreman. You keep feeding a broken machine. There's a metaphor here."},
            {Speaker::Wilson,  "It's not broken. It hates him specifically. Watch — it'll work for me."},
            {Speaker::Foreman, "If you insert a dollar and that machine gives you a Snickers, I quit."},
            {Speaker::Cuddy,   "Can we focus? Maintenance comes Thursday. Stop feeding it."},
            {Speaker::House,   "Cuddy, the man wants a Snickers. You can't stand between Foreman and dignity."},
            {Speaker::Foreman, "I don't even want a Snickers anymore. I want my dollar."},
            {Speaker::House,   "That's exactly what the machine wants you to say."},
            {Speaker::House,   "Maintenance won't come Thursday. They never come Thursday. Welcome to the universe."},
        },

        // Script 5 — Insurance auditor on the floor
        {
            {Speaker::Cuddy,   "The insurance auditor is on the floor. Be normal for forty-five minutes."},
            {Speaker::House,   "I have no idea what that word means."},
            {Speaker::Wilson,  "Don't bill anything as 'exploratory chaos' today. They flagged that last quarter."},
            {Speaker::Cuddy,   "Don't bill anything as 'creative diagnostics' either. Or 'gut feeling'."},
            {Speaker::House,   "What if I bill it as 'differential intuition'? More clinical."},
            {Speaker::Foreman, "He's serious. He'll do it."},
            {Speaker::Cuddy,   "House. I will personally hide your cane."},
            {Speaker::House,   "Threats. From the woman who can't find her own stapler."},
            {Speaker::House,   "Fine. Today I'll bill in Latin. The auditor will assume I'm respectable."},
        },

        // Script 6 — House's cane is "missing"
        {
            {Speaker::House,   "Someone took my cane. I left it leaning against the elevator."},
            {Speaker::Wilson,  "Nobody took your cane. You probably left it in the cafeteria. Again."},
            {Speaker::House,   "Wilson. You're suspect number one. You've been resentful for years."},
            {Speaker::Cuddy,   "It's behind your office door. I saw it when I left the chart there."},
            {Speaker::House,   "Convenient explanation, Cuddy. Almost like you planted it."},
            {Speaker::Foreman, "Chase has the spare. He's been holding it hostage since the Vegas trip."},
            {Speaker::House,   "Chase has my SPARE cane? That changes everything."},
            {Speaker::Wilson,  "He really has a spare cane?"},
            {Speaker::House,   "Of course I have a spare cane. I'm not an amateur."},
            {Speaker::House,   "I'm finding Chase. Someone clear my morning. Cuddy — that's now your job."},
        },

        // Script 7 — Chase did something stupid overnight
        {
            {Speaker::Foreman, "Chase intubated the wrong patient overnight. Room 312, not 314."},
            {Speaker::House,   "Wrong PATIENT? Or wrong ROOM with the same patient who keeps wandering?"},
            {Speaker::Foreman, "Wrong patient. Different patient. Now both intubated. We extubated one."},
            {Speaker::Wilson,  "Is the wrong-room patient okay?"},
            {Speaker::Foreman, "Furious. The right-room patient is also furious."},
            {Speaker::House,   "Furious patients are healthy patients. Chase saved two lives by accident."},
            {Speaker::Cuddy,   "I am not signing off on that framing, House."},
            {Speaker::House,   "Cuddy, paperwork is just a story we tell ourselves. Let me write Chase's."},
            {Speaker::House,   "I'll start with: 'Aggressive proactive airway management on two patients simultaneously.' Bills better."},
        },

        // Script 8 — Cuddy needs House at a black-tie gala
        {
            {Speaker::Cuddy,   "House. Black-tie fundraiser. Saturday. You're going."},
            {Speaker::House,   "I'm allergic to bow ties. And rich people. And Saturdays."},
            {Speaker::Wilson,  "He's going. I'll buy the tie. I'll drag him by the ear if I have to."},
            {Speaker::Cuddy,   "Donors want to meet 'the diagnostic genius.' You'll smile for three hours."},
            {Speaker::House,   "I'll smile for ninety seconds. Then I'll insult a banker. That's my range."},
            {Speaker::Foreman, "I'll pay to watch this."},
            {Speaker::House,   "Cuddy. You want me there because the donors want a freak show. Admit it."},
            {Speaker::Cuddy,   "I want you there because they wrote a check for an MRI you've been begging for."},
            {Speaker::House,   "Fine. New MRI for a bow tie. Worst Faustian bargain I've ever made."},
        },

        // Script 9 — House's TV is broken at home (actual angst)
        {
            {Speaker::House,   "My TV died last night. Mid-episode. The screen just went black."},
            {Speaker::Wilson,  "I'm sorry to hear that. Genuinely. Are you okay?"},
            {Speaker::House,   "Wilson, I missed the reveal. I'll never know who killed her."},
            {Speaker::Cuddy,   "There's a thing called the internet. You can look it up."},
            {Speaker::House,   "Cuddy. You don't look up a finale. You earn it."},
            {Speaker::Foreman, "He looks legitimately sad. This is unsettling."},
            {Speaker::House,   "I am sad. I am a man without television. I am incomplete."},
            {Speaker::Wilson,  "I'll lend you my old set tonight. Stop moping."},
            {Speaker::House,   "Wilson. You are a saint and a profoundly broken individual."},
        },

        // Script 10 — New intern is too eager / asked for House specifically
        {
            {Speaker::Wilson,  "There's a new intern. She's read every paper you've ever written."},
            {Speaker::House,   "Tell her she's wasting her life. Then tell her where I keep the Vicodin."},
            {Speaker::Cuddy,   "He is not telling her where you keep the Vicodin."},
            {Speaker::Wilson,  "She asked specifically for your team. She used the word 'mentor.'"},
            {Speaker::House,   "Mentor. That's a curse word. Who taught her that?"},
            {Speaker::Foreman, "We taught her that. We thought it'd be funny."},
            {Speaker::House,   "Foreman. Bringing a new puppy to my office is not funny. It's animal cruelty."},
            {Speaker::House,   "Tell her my first lesson is 'leave.' If she stays, I'll consider the second."},
        },

        // Script 11 — Cameron's birthday
        {
            {Speaker::Cuddy,   "It's Cameron's birthday today. She brought cake. Be a person for an hour."},
            {Speaker::House,   "What kind of cake."},
            {Speaker::Wilson,  "Lemon. With raspberry filling. She remembered you complained about chocolate."},
            {Speaker::House,   "...Lemon. Fine. I'll have one piece and one civilized sentence."},
            {Speaker::Foreman, "One civilized sentence. House, I'll bet you twenty bucks you can't."},
            {Speaker::House,   "Foreman. You're on. I will say something nice. And mean it for two seconds."},
            {Speaker::Cuddy,   "If this destabilizes him, I'm holding all of you responsible."},
            {Speaker::House,   "Don't worry. I've already drafted the sentence. It's about her hair."},
        },

        // Script 12 — Experiment House left running in the office overnight
        {
            {Speaker::Cuddy,   "There is a centrifuge in your office. It's been running since 2 a.m."},
            {Speaker::House,   "It needs to run. That's how it works. The clue is in the name."},
            {Speaker::Wilson,  "He has six urine samples in there. I cleaned the office floor at five."},
            {Speaker::House,   "Wilson. You cleaned my floor? That's a violation of three different friendships."},
            {Speaker::Foreman, "Why are there six urine samples, House. We have one patient."},
            {Speaker::House,   "Foreman. Diagnostic medicine is a science of comparisons."},
            {Speaker::Cuddy,   "Whose urine, House. WHOSE."},
            {Speaker::House,   "I won't answer that on advice of counsel I haven't hired yet."},
            {Speaker::House,   "Anyway, Cuddy. You should leave my office now. The centrifuge needs its space."},
        },

    };

    static std::mt19937 rng(static_cast<unsigned>(std::time(nullptr)));
    std::uniform_int_distribution<size_t> dist(0, pool.size() - 1);
    return pool[dist(rng)];
}
